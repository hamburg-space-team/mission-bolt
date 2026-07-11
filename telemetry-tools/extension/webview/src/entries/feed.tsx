import { createRoot } from "react-dom/client";
import "../assets/theme.css";
import { Feed } from "../components/Feed/Feed";

createRoot(document.getElementById("root")!).render(<Feed />);
