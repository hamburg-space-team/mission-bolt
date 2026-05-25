package main

import (
	"context"
	"fmt"
	"io"
	"log"
	"net"
	"net/http"
	"strings"
	"sync"
	"time"

	"github.com/gin-contrib/cors"
	"github.com/gin-gonic/gin"
)

const (
	httpAddr   = ":8000"
	sensorHost = "127.0.0.1:54321"
)

// Broker is a simple in-process pub/sub hub for SSE clients.
//
// - `clients` holds a map of subscriber channels keyed by an integer id.
// - `nextID` generates unique subscriber ids.
// - `mu` protects the map for concurrent Subscribe/Unsubscribe/Broadcast calls.
//
// This pattern keeps the server single-process: socketReader publishes
// sensor messages into the Broker and HTTP handlers subscribe to receive
// messages for each connected SSE client.
type Broker struct {
	mu      sync.RWMutex
	nextID  int
	clients map[int]chan string
}

func NewBroker() *Broker {
	return &Broker{
		clients: make(map[int]chan string),
	}
}

func (b *Broker) Subscribe() (int, chan string) {
	// Create a new buffered channel for this subscriber and register it.
	// Buffered channels prevent a single slow client from blocking the
	// broadcaster; we drop messages when the buffer is full (see Broadcast).
	b.mu.Lock()
	defer b.mu.Unlock()

	id := b.nextID
	b.nextID++
	ch := make(chan string, 256)
	b.clients[id] = ch
	return id, ch
}

func (b *Broker) Unsubscribe(id int) {
	// Remove subscriber and close its channel so any goroutines reading
	// from the channel can terminate cleanly.
	b.mu.Lock()
	defer b.mu.Unlock()

	if ch, ok := b.clients[id]; ok {
		delete(b.clients, id)
		close(ch)
	}
}

func (b *Broker) Broadcast(message string) {
	// Send `message` to every subscriber channel. If a subscriber's buffer
	// is full we drop the message for that subscriber to avoid blocking
	// the whole broadcast (tradeoff: best-effort delivery).
	b.mu.RLock()
	defer b.mu.RUnlock()

	for _, ch := range b.clients {
		select {
		case ch <- message:
		default:
			// Drop if a client is too slow; we want the broker to stay responsive.
		}
	}
}

func socketReader(ctx context.Context, broker *Broker) {
	// Continuously attempt to connect to the upstream sensor TCP server.
	// If the connection dies we retry with a backoff. Each successful read
	// is published into the Broker for SSE clients.
	for {
		select {
		case <-ctx.Done():
			return
		default:
		}

		log.Printf("Attempting to connect to sensor server at %s...", sensorHost)
		conn, err := net.Dial("tcp", sensorHost)
		if err != nil {
			log.Printf("Connection failed: %v. Retrying in 2 seconds...", err)
			select {
			case <-ctx.Done():
				return
			case <-time.After(2 * time.Second):
			}
			continue
		}

		log.Println("Successfully connected to sensor!")
		_ = conn.SetReadDeadline(time.Time{})
		buf := make([]byte, 1024)

		for {
			select {
			case <-ctx.Done():
				_ = conn.Close()
				return
			default:
			}

			n, readErr := conn.Read(buf)
			if readErr != nil {
				log.Printf("Sensor connection closed/read error: %v", readErr)
				_ = conn.Close()
				break
			}

			if n == 0 {
				continue
			}

			// Normalize the message and publish it. Messages are expected to
			// be newline-terminated or framed by the sensor sender; this code
			// performs a simple trim and then broadcasts raw payload strings.
			message := strings.TrimSpace(string(buf[:n]))
			if message != "" {
				broker.Broadcast(message)
			}
		}
	}
}

func main() {
	broker := NewBroker()
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	go socketReader(ctx, broker)
	log.Println("Background socket reader started.")
	// Use Gin for HTTP + SSE routing (simpler handler ergonomics).
	// The server's responsibilities:
	// - run a background TCP socket reader that publishes sensor messages
	//   into the in-process Broker
	// - expose an `/events` SSE endpoint that subscribes each client to
	//   the Broker and streams messages as they arrive
	r := gin.Default()
	// Allow the Vite dev server to call the SSE endpoint directly from
	// http://localhost:5173 during development.
	r.Use(cors.New(cors.Config{
		AllowOrigins:     []string{"http://localhost:5173"},
		AllowMethods:     []string{"GET", "OPTIONS"},
		AllowHeaders:     []string{"Origin", "Content-Type", "Accept"},
		ExposeHeaders:    []string{"Content-Length"},
		AllowCredentials: false,
		MaxAge:           12 * time.Hour,
	}))

	// Note: the previous base HTML route has been removed — clients should
	// connect directly to `/events` via EventSource. Keeping the server
	// minimal reduces surface area.

	r.GET("/events", func(c *gin.Context) {
		flusher, ok := c.Writer.(http.Flusher)
		if !ok {
			c.String(http.StatusInternalServerError, "Streaming unsupported")
			return
		}

		c.Writer.Header().Set("Content-Type", "text/event-stream")
		c.Writer.Header().Set("Cache-Control", "no-cache")
		c.Writer.Header().Set("Connection", "keep-alive")
		c.Writer.Header().Set("X-Accel-Buffering", "no")

		// Each request subscribes to the Broker and receives a dedicated
		// buffered channel. We defer Unsubscribe so the Broker removes the
		// client and closes the channel when the handler returns (client
		// disconnect or error).
		id, ch := broker.Subscribe()
		defer broker.Unsubscribe(id)

		// initial flush to ensure headers are sent promptly
		flusher.Flush()

		// keepAlive sends periodic SSE comments to keep intermediaries
		// from closing the connection when idle.
		keepAlive := time.NewTicker(20 * time.Second)
		defer keepAlive.Stop()

		ctxReq := c.Request.Context()

		// c.Stream repeatedly calls the provided function; returning false
		// stops the stream and finishes the request. We use select to wait
		// on either new messages, client cancellation, or the keepalive
		// ticker.
		c.Stream(func(w io.Writer) bool {
			select {
			case <-ctxReq.Done():
				return false
			case msg, ok := <-ch:
				if !ok {
					return false
				}
				// send as SSE data field (event name: "message")
				c.SSEvent("message", msg)
				flusher.Flush()
				return true
			case <-keepAlive.C:
				_, _ = fmt.Fprint(c.Writer, ": heartbeat\n\n")
				flusher.Flush()
				return true
			}
		})
	})

	log.Printf("SSE server (Gin) listening on %s", httpAddr)
	if err := r.Run(httpAddr); err != nil {
		log.Fatalf("Server stopped: %v", err)
	}
}

// Note: No base HTML is served by this server anymore. Frontend clients
// should connect directly to `/events` using `new EventSource('/events')`.
