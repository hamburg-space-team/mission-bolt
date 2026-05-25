import { Button } from "@/components/ui/button";
import logo from "@/assets/rx_bolt_logo_thor_v4.png";

const navItems = ["BTC", "Experiment 1", "Experiment 2", "Experiment 3"];

function Navbar({ activeTab, onTabChange }) {
  return (
    <header className="sticky top-0 z-40 border-b border-border/60 bg-background/80 backdrop-blur supports-[backdrop-filter]:bg-background/60">
      <div className="flex w-full items-center justify-between gap-3 px-4 py-2 sm:px-6 lg:px-10">
        <div className="flex items-center gap-2">
          <img
            src={logo}
            alt="THHORS-BOLT logo"
            className="size-8 rounded-lg border border-border bg-muted object-contain p-0.5 shadow-sm"
          />
          <div className="leading-tight">
            <p className="text-sm font-semibold text-foreground">THHORS-BOLT</p>
          </div>
        </div>

        <nav
          aria-label="Primary"
          className="flex flex-wrap items-center justify-end gap-2"
        >
          {navItems.map((item) => (
            <Button
              key={item}
              type="button"
              variant={activeTab === item ? "secondary" : "ghost"}
              size="xs"
              className="px-2 text-muted-foreground hover:text-foreground"
              aria-pressed={activeTab === item}
              onClick={() => onTabChange(item)}
            >
              {item}
            </Button>
          ))}
        </nav>
      </div>
    </header>
  );
}

export default Navbar;
