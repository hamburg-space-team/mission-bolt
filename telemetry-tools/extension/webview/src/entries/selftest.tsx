import { createRoot } from "react-dom/client";
import "../assets/theme.css";
import { SelfTest } from "../components/SelfTest/SelfTest";

createRoot(document.getElementById("root")!).render(<SelfTest />);
