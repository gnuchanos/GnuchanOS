import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import "./monacoSetup";
import "./styles.css";

ReactDOM.createRoot(document.getElementById("root")!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

/* React mount olunca baslangic splash'ini kaldir (index.html'deki
 * #boot-splash). Daha once bundle yuklenene kadar beyaz ekran gorunuyordu;
 * splash bunu orter. */
requestAnimationFrame(() => {
  const splash = document.getElementById("boot-splash");
  if (splash) splash.classList.add("done");
});
