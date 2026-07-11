import { createRoot } from "react-dom/client";
import "tabulator-tables/dist/css/tabulator.min.css";
import "../assets/theme.css";
import "../assets/grid.css";
import { Packets } from "../components/Packets/Packets";

createRoot(document.getElementById("root")!).render(<Packets />);
