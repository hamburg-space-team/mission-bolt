import { clsx } from "clsx";
import { twMerge } from "tailwind-merge";

export function cn(...inputs) {
  return twMerge(clsx(inputs));
}

export const getCSSVar = (name) => {
  return getComputedStyle(document.documentElement)
    .getPropertyValue(name)
    .trim();
};

// Formats the shadcn HSL variable into a CSS HSL string
export const getHSL = (varName) => {
  const value = getCSSVar(varName);
  return value ? `hsl(${value})` : null;
};
