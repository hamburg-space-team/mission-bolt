// vsce -> undici does `MakeTypeAssertion(File)` at import time. Node 20+ has
// File/Blob as globals; Node 18 (this devcontainer) keeps them in `buffer`
// only, so undici dies with "ReferenceError: File is not defined". These are
// the same built-in classes, just published to globalThis.
// Delete this shim and the --require once the devcontainer is on Node 20+.
const { File, Blob } = require("buffer");
if (typeof globalThis.File === "undefined") globalThis.File = File;
if (typeof globalThis.Blob === "undefined") globalThis.Blob = Blob;
