import { createRoot } from "react-dom/client";
import "../assets/theme.css";
import { Experiment } from "../components/Experiment/Experiment";

createRoot(document.getElementById("root")!).render(<Experiment />);
