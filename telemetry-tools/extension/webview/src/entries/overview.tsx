import { createRoot } from "react-dom/client";
import "../assets/theme.css";
import { Overview } from "../components/Overview/Overview";

createRoot(document.getElementById("root")!).render(<Overview />);
