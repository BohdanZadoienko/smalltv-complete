// Restaurar backup do painel (web/index.html): cada etapa roda à parte e uma falha não impede as outras.
// Extrai o runSteps real do painel, confere a ordem/contabilidade e que todo texto novo tem inglês e espanhol.
const fs = require('fs'), path = require('path'), assert = require('assert');
const html = fs.readFileSync(path.join(__dirname, '../web/index.html'), 'utf8');
const i = html.indexOf('async function runSteps('), runSteps = new Function('return ' + html.slice(i, html.indexOf('\n', i)))();
(async () => {
  const ran = [];
  const R = await runSteps([
    ['Mostradores', async () => ran.push('m')],
    ['Ajustes', async () => ran.push('a')],
    ['Histórico', async () => { ran.push('h'); throw 'Backup invalido'; }],
    ['Avisos', async () => { ran.push('v'); throw new Error('falhou'); }],
    ['Fotos e GIFs', async () => ran.push('f')]]);
  assert.deepStrictEqual(ran, ['m', 'a', 'h', 'v', 'f']);   // a falha do histórico não impediu as fotos
  assert.deepStrictEqual(R.ok, ['Mostradores', 'Ajustes', 'Fotos e GIFs']);
  assert.deepStrictEqual(R.bad, [['Histórico', 'Backup invalido'], ['Avisos', 'falhou']]);
  assert.deepStrictEqual((await runSteps([])).bad, []);
  // o handler usa as cinco etapas, o cartão de migração depende só das fotos e os textos estão traduzidos
  const h = html.slice(html.indexOf("$('rbk').onchange"), html.indexOf('// ---------- avisos de fim de fase'));
  for (const n of ['Mostradores', 'Ajustes', 'Histórico', 'Avisos', 'Fotos e GIFs']) assert(h.includes(`['${n}',`), n);
  assert(h.includes("if(!R.bad.some(x=>x[0]=='Fotos e GIFs')&&st&&st.fs&&st.fs.mig)migDone()"));
  const dict = JSON.parse(fs.readFileSync(path.join(__dirname, '../web/i18n.json'), 'utf8')).dict;
  const keys = ['Mostradores', 'Ajustes', 'Histórico', 'Avisos', 'Fotos e GIFs', 'Backup restaurado com falhas.', 'Entrou: {0}', 'Falhou:',
    '{0} mostrador(es) não estão na galeria e precisam ser instalados de novo pelo arquivo', '{0} foto(s)/GIF(s) não couberam e ficaram de fora'];
  for (const k of keys) assert(dict[k] && dict[k].length == 2 && dict[k].every(x => x), 'sem tradução: ' + k);
  console.log('painel: restaurar backup segue após falha de uma etapa e resume o que entrou e o que falhou');
})();
// Filtro dos mostradores (web/index.html): os nativos precisam de estilos na ordem de faces_list.h, só o Flip anima,
// o filtro lê o campo anim do catálogo e todo texto novo tem inglês e espanhol.
{
  const reg = fs.readFileSync(path.join(__dirname, '../smalltv-pomodoro/faces_list.h'), 'utf8'), ids = [...reg.replace(/^\/\/.*$/gm, '').matchAll(/X\(([A-Z_]+), /g)].map(m => m[1]);
  const BTAG = new Function('return ' + html.match(/BTAG=(\[[^\]]*\])/)[1])(), BANIM = new Function('return ' + html.match(/BANIM=(\[[^\]]*\])/)[1])();
  const sty = ['pixel', 'digital', 'analogico', 'retro', 'esportivo'];
  assert.strictEqual(BTAG.length, ids.length, 'um mostrador novo em faces_list.h precisa de estilos em BTAG (web/index.html)');
  assert(BTAG.every(s => s.split(' ').every(x => sty.includes(x))), 'estilo desconhecido em BTAG');
  assert.deepStrictEqual(BANIM.map(i => ids[i]), ['FLIP']);
  const mtok = new Function('return ' + html.match(/mtok=(m=>[^,]*),ftok/)[1])();
  assert.strictEqual(mtok(['pixel digital', 1]), 'pixel digital anim');
  assert.strictEqual(mtok(['retro', 0]), 'retro');
  assert.strictEqual(mtok(undefined), '');   // pacote fora do catálogo: sem estilo nem animação
  assert(html.includes("e.anim?1:0"), 'setMeta deve guardar se o pacote do catálogo é animado');
  const dict = JSON.parse(fs.readFileSync(path.join(__dirname, '../web/i18n.json'), 'utf8')).dict;
  for (const k of ['Todos', 'Nativos', 'Baixados', 'Animados', 'Digital', 'Pixel art', 'Analógico', 'Retrô', 'Esportivo', 'Nenhum mostrador neste filtro.',
    'Nenhum mostrador neste filtro ainda. Abra a galeria online logo abaixo para ver mais.'])
    assert(dict[k] && dict[k].length == 2 && dict[k].every(x => x), 'sem tradução: ' + k);
  console.log('painel: filtro dos mostradores com estilos para os ' + ids.length + ' nativos, só o Flip animado e textos traduzidos');
}
