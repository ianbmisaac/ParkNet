(() => {
  const canvas = document.getElementById("parkView");
  const ctx = canvas.getContext("2d");
  const status = document.getElementById("parkViewStatus");
  if (!ctx) {
    status.textContent = "Vista 3D no disponible en este navegador";
    return;
  }

  const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");
  const stalls = [
    { x: -2.65, color: ["#76d0ff", "#287fbe", "#1e5e94"] },
    { x: -.15, color: ["#ffbf76", "#d67730", "#9e4c24"] },
    { x: 2.35, color: ["#c6a6ff", "#885bcc", "#603f9b"] }
  ];
  const barrier = { from: 0, to: 0, start: 0, duration: 900 };
  const cars = stalls.map(() => ({ from: 0, to: 0, start: 0, duration: 3200, reported: false }));
  let synced = false;
  let frame = 0;

  function project([x, y, z]) {
    return [380 + 57 * x - 33 * z, 218 + 17 * x + 29 * z - 57 * y];
  }

  function polygon(points, color) {
    ctx.beginPath();
    points.forEach((point, index) => {
      const [x, y] = project(point);
      if (index === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.closePath();
    ctx.fillStyle = color;
    ctx.fill();
  }

  function floor(x, z, width, length, color, y = 0) {
    polygon([[x, y, z], [x + width, y, z], [x + width, y, z + length],
      [x, y, z + length]], color);
  }

  function label(text, x, z, color) {
    const [px, py] = project([x, 0, z]);
    ctx.fillStyle = color;
    ctx.fillText(text, px, py);
  }

  function box(faces, transform, x, y, z, w, h, l, colors) {
    const corners = [
      [x, y, z], [x + w, y, z], [x + w, y, z + l], [x, y, z + l],
      [x, y + h, z], [x + w, y + h, z], [x + w, y + h, z + l], [x, y + h, z + l]
    ].map(transform);
    const sides = [
      [[4, 5, 6, 7], colors[0]], [[1, 2, 6, 5], colors[1]],
      [[2, 3, 7, 6], colors[2]], [[0, 1, 5, 4], colors[2]],
      [[3, 0, 4, 7], colors[1]], [[3, 2, 1, 0], colors[1]]
    ];
    for (const [indices, color] of sides) {
      const points = indices.map(index => corners[index]);
      const [a, b, c] = points.map(project);
      const winding = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
      if (winding <= 0) continue;
      const depth = points.reduce((sum, [px, py, pz]) => sum + px * .58 + py * .68 + pz, 0);
      faces.push({ points, color, depth });
    }
  }

  function drawCar(faces, stall, progress, index) {
    const lane = 1.45 + index * .31;
    const radius = 1.15;
    const entry = 10.8;
    const turnX = stall.x + radius;
    const turnZ = lane - radius;
    const straight = entry - turnX;
    const arc = Math.PI / 2 * radius;
    const distance = progress * (straight + arc + turnZ + 1.65);
    const turn = Math.min(Math.PI / 2, Math.max(0, (distance - straight) / radius));
    const x = turnX - radius * Math.sin(turn) + Math.max(0, straight - distance);
    const z = turnZ + radius * Math.cos(turn) - Math.max(0, distance - straight - arc);
    const angle = Math.PI / 2 - turn;
    const sin = Math.sin(angle), cos = Math.cos(angle);
    const transform = ([px, py, pz]) => [x + px * cos + pz * sin, py, z - px * sin + pz * cos];
    const dark = ["#27323c", "#151a21", "#0b0e13"];
    box(faces, transform, -.46, .13, -.88, .92, .34, 1.76, stall.color);
    box(faces, transform, -.34, .47, -.45, .68, .28, .91, ["#89a7bb", "#324f63", "#223647"]);
    for (const wheelX of [-.51, .38]) {
      for (const wheelZ of [-.64, .38]) {
        box(faces, transform, wheelX, .02, wheelZ, .13, .28, .31, dark);
      }
    }
    for (const lightX of [-.37, .24]) {
      box(faces, transform, lightX, .29, -.9, .13, .09, .04, ["#fff0ad", "#e8cd75", "#fff0ad"]);
      box(faces, transform, lightX, .29, .86, .13, .09, .04, ["#ff8272", "#ae3938", "#f55e56"]);
    }
  }

  function value(motion, now) {
    const t = Math.min(1, (now - motion.start) / motion.duration);
    return motion.from + (motion.to - motion.from) * (t * t * (3 - 2 * t));
  }

  function setTarget(motion, target, now, immediate) {
    if (!immediate && motion.to === target) return;
    motion.from = immediate ? target : value(motion, now);
    motion.to = target;
    motion.start = now;
  }

  function draw(now) {
    frame = 0;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    const gradient = ctx.createLinearGradient(0, 0, 0, canvas.height);
    gradient.addColorStop(0, "#172535");
    gradient.addColorStop(1, "#0a1018");
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    floor(-4.2, -3.6, 10.85, 7.1, "#080e16", -.27);
    floor(-4.2, -3.6, 10.85, 7.1, "#384956");
    floor(-4, -3.4, 10.65, 6.7, "#222d37");
    floor(-4, .47, 10.65, 2.58, "#303b46");
    for (let x = -3.75; x < 6.2; x += 1.06) floor(x, 1.76, .56, .045, "#82909b");
    floor(-4, .41, 10.65, .038, "#536471");
    floor(-4, 3.02, 10.65, .04, "#536471");

    ctx.font = "bold 28px system-ui, sans-serif";
    ctx.textAlign = "center";
    const faces = [];
    const fixed = point => point;
    stalls.forEach((stall, index) => {
      const car = cars[index];
      const reported = synced && car.reported;
      const tint = reported ? (car.to ? "#f48e7f" : "#77ddad") : "#8da0af";
      const fill = reported ? (car.to ? "#473039" : "#233f3c") : "#303f4c";
      floor(stall.x - 1.05, -3.12, 2.1, 2.94, tint);
      floor(stall.x - .98, -3.05, 1.96, 2.8, fill, .003);
      label("P" + (index + 1), stall.x, -.43, "#edf5fc");
      const progress = value(car, now);
      if (progress > 0) drawCar(faces, stall, progress, index);
    });
    ctx.font = "bold 16px system-ui, sans-serif";
    label("ACCESO", 5.78, 2.8, "#c0ccd6");

    box(faces, fixed, -3.9, 0, -3.5, 7.2, .34, .24, ["#7290a7", "#526d80", "#425867"]);
    box(faces, fixed, 3.65, 0, 3.1, .52, .13, .38, ["#a2b2bf", "#647989", "#536472"]);
    box(faces, fixed, 3.79, .13, 3.15, .24, .9, .25, ["#ffc77c", "#b66f35", "#925325"]);
    const angle = value(barrier, now) * Math.PI / 2;
    const arm = ([x, y, z]) => [
      3.91 + x, .99 + y * Math.cos(angle) - z * Math.sin(angle),
      3.25 + z * Math.cos(angle) + y * Math.sin(angle)
    ];
    for (let i = 0; i < 8; i++) {
      const stripe = i % 2 ? ["#f57569", "#be3e41", "#962e35"] : ["#e5edf1", "#a6b7c5", "#8396a6"];
      box(faces, arm, -.055, -.052, -(i + 1) * .34, .11, .104, .34, stripe);
    }
    faces.sort((a, b) => a.depth - b.depth);
    for (const face of faces) polygon(face.points, face.color);

    if ([barrier, ...cars].some(motion =>
      motion.from !== motion.to && now < motion.start + motion.duration)) {
      frame = requestAnimationFrame(draw);
    }
  }

  function render() {
    if (!frame) frame = requestAnimationFrame(draw);
  }

  window.ParkNetView = {
    update(state) {
      if (!Array.isArray(state.ocupadas) ||
          ![0, 1].includes(state.ocupadas[0]) || ![0, 1].includes(state.ocupadas[1]) ||
          !["ABIERTA", "CERRADA"].includes(state.barrera)) return;

      const now = performance.now();
      const immediate = !synced || reducedMotion.matches;
      setTarget(barrier, Number(state.barrera === "ABIERTA"), now, immediate);
      cars.forEach((car, index) => {
        const target = state.ocupadas[index];
        const reported = target === 0 || target === 1;
        setTarget(car, reported ? target : 0, now, immediate || !reported || !car.reported);
        car.reported = reported;
      });
      const label = cars[2].reported ? "Sincronizado con la placa" : "P3 sin datos de la placa";
      if (status.textContent !== label) status.textContent = label;
      synced = true;
      render();
    },
    disconnect() {
      synced = false;
      status.textContent = "Esperando datos de la placa";
      render();
    }
  };
  render();
})();
