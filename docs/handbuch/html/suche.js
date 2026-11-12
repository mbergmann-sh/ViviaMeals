
(function () {
  var feld = document.getElementById('suche');
  if (!feld || !window.VM_INDEX) return;
  var main = document.querySelector('main');
  var original = null;
  feld.addEventListener('input', function () {
    var q = feld.value.trim().toLowerCase();
    if (!q) { if (original !== null) { main.innerHTML = original; original = null; } return; }
    if (original === null) original = main.innerHTML;
    var woerter = q.split(/\s+/), treffer = [];
    VM_INDEX.forEach(function (e) {
      var t = (e.t + ' ' + e.x).toLowerCase(), ok = true;
      woerter.forEach(function (w) { if (t.indexOf(w) < 0) ok = false; });
      if (ok) treffer.push(e);
    });
    var h = '<h1>Suche: „' + feld.value.replace(/[<&]/g, '') + '“</h1>';
    if (!treffer.length) h += '<p>Keine Treffer.</p>';
    else h += '<ol id="treffer">' + treffer.slice(0, 60).map(function (e) {
      return '<li><a href="' + e.u + '">' + e.t + '</a> <small>– ' + e.k + '</small></li>'; }).join('') + '</ol>';
    main.innerHTML = h;
  });
})();
