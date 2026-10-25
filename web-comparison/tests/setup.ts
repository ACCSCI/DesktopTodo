Object.defineProperty(window, 'matchMedia', { value: (media: string) => ({
  media, matches: false, onchange: null,
  addEventListener() {}, removeEventListener() {},
  addListener() {}, removeListener() {}, dispatchEvent() { return true; }
}) });
