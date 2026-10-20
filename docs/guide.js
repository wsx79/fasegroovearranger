'use strict';
const field = document.querySelector('#search');
const count = document.querySelector('#count');
const blocks = [...document.querySelectorAll('article h1,article h2,article h3,article p,article li,article tr,article blockquote')];
let matches = [], active = -1, size = 16;
function move(direction) {
  if (!matches.length) { count.textContent = field.value ? '0 / 0' : ''; return; }
  active = (active + direction + matches.length) % matches.length;
  matches[active].scrollIntoView({ block: 'center', behavior: 'smooth' });
  count.textContent = `${active + 1} / ${matches.length}`;
}
field.addEventListener('input', () => {
  const q = field.value.trim().toLocaleLowerCase();
  matches = blocks.filter(el => q && el.textContent.toLocaleLowerCase().includes(q));
  blocks.forEach(el => el.classList.toggle('match', matches.includes(el)));
  active = -1; move(1);
});
field.addEventListener('keydown', e => { if (e.key === 'Enter') { e.preventDefault(); move(e.shiftKey ? -1 : 1); } });
document.querySelector('#previous').onclick = () => move(-1);
document.querySelector('#next').onclick = () => move(1);
for (const [id, delta] of [['smaller', -1], ['larger', 1]]) {
  document.querySelector('#' + id).onclick = () => { size = Math.max(13, Math.min(21, size + delta)); document.documentElement.style.setProperty('--reading-size', size + 'px'); };
}
document.addEventListener('keydown', e => { if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'f') { e.preventDefault(); field.focus(); } });
