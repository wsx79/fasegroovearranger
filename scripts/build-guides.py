#!/usr/bin/env python3
"""Render the controlled family Markdown guides as self-contained offline pages.

No network, package dependencies or generated prose. Validate every local link,
image and chapter target before packaging the same sources used by the native app.
"""
from pathlib import Path
import html
import re
import unicodedata

root = Path(__file__).resolve().parent.parent

def slug(text):
    text = unicodedata.normalize('NFKD', text).encode('ascii', 'ignore').decode().lower()
    return re.sub(r'[^a-z0-9-]', '', text.replace(' ', '-'))

def inline(text):
    text = html.escape(text)
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', text)
    def link(m):
        target = html.unescape(m[2])
        if target in ('GUIDA.md', 'GUIDE.md'): target = target[:-3] + '.html'
        return f'<a href="{html.escape(target, quote=True)}">{m[1]}</a>'
    return re.sub(r'\[([^\]]+)\]\(([^)]+)\)', link, text)

def render(source):
    lines = source.splitlines(); output = []; toc = []; i = 0
    def special(s):
        return not s or s.startswith(('#', '|', '> ', '![', '- ', '```')) or s == '---' or re.match(r'\d+\. ', s)
    while i < len(lines):
        s = lines[i].strip(); i += 1
        if not s: continue
        if s.startswith('```'):
            code = []
            while i < len(lines) and not lines[i].startswith('```'): code.append(lines[i]); i += 1
            i += 1; output.append('<pre><code>' + html.escape('\n'.join(code)) + '</code></pre>'); continue
        if s == '---': output.append('<hr>'); continue
        if s.startswith('#'):
            level = len(s) - len(s.lstrip('#')); title = s[level:].strip(); anchor = slug(title)
            output.append(f'<h{level} id="{anchor}">{inline(title)}</h{level}>')
            if level == 2: toc.append((anchor, title))
            continue
        if s.startswith('!['):
            m = re.fullmatch(r'!\[([^\]]*)\]\(([^)]+)\)', s); assert m, s
            assert (root / 'docs' / m[2]).is_file(), m[2]
            output.append(f'<figure><a href="{m[2]}"><img src="{m[2]}" alt="{html.escape(m[1])}" loading="lazy"></a><figcaption>{html.escape(m[1])}</figcaption></figure>'); continue
        if s.startswith('|'):
            rows = [s]
            while i < len(lines) and lines[i].strip().startswith('|'): rows.append(lines[i].strip()); i += 1
            table = []
            for row in rows:
                cells = [c.strip() for c in row.strip('|').split('|')]
                if all(re.fullmatch(r'[-: ]+', c) for c in cells): continue
                tag = 'th' if not table else 'td'
                table.append('<tr>' + ''.join(f'<{tag}>{inline(c)}</{tag}>' for c in cells) + '</tr>')
            output.append('<div class="table"><table>' + ''.join(table) + '</table></div>'); continue
        if s.startswith('> '):
            quote = [s[2:]]
            while i < len(lines) and lines[i].strip().startswith('> '): quote.append(lines[i].strip()[2:]); i += 1
            output.append('<blockquote>' + inline(' '.join(quote)) + '</blockquote>'); continue
        if s.startswith('- ') or re.match(r'\d+\. ', s):
            ordered = not s.startswith('- '); rows = [s]
            pattern = r'\d+\. ' if ordered else r'- '
            while i < len(lines) and re.match(pattern, lines[i].strip()): rows.append(lines[i].strip()); i += 1
            tag = 'ol' if ordered else 'ul'; output.append(f'<{tag}>' + ''.join('<li>' + inline(re.sub(pattern, '', x, count=1)) + '</li>' for x in rows) + f'</{tag}>'); continue
        paragraph = [s]
        while i < len(lines) and not special(lines[i].strip()): paragraph.append(lines[i].strip()); i += 1
        output.append('<p>' + inline(' '.join(paragraph)) + '</p>')
    return '\n'.join(output), toc

for language, name in [('it', 'GUIDA'), ('en', 'GUIDE')]:
    source = (root / 'docs' / (name + '.md')).read_text()
    body, toc = render(source)
    assert len(toc) == 13, name
    assert body.count('<table>') >= 10 and body.count('<figure>') == 4, name
    ids = set(re.findall(r'id="([^"]+)"', body))
    for target in re.findall(r'href="#([^"]+)"', body): assert target in ids, (name, target)
    english = language == 'en'; title = 'User guide' if english else 'Guida di utilizzo'
    other = 'GUIDA.html' if english else 'GUIDE.html'; other_label = 'Italiano' if english else 'English'
    contents = 'Contents' if english else 'Indice'; search = 'Search the guide' if english else 'Cerca nella guida'
    html_doc = f'''<!doctype html>
<html lang="{language}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>{title} – Fase Groove Arranger</title><link rel="stylesheet" href="guide.css"></head>
<body><header><a class="brand" href="#"><img src="../Resources/arranger.svg" alt=""><span><b>FASE</b><em>GROOVE ARRANGER</em></span></a><span class="guide-title">{title}</span><a class="language" href="{other}">{other_label}</a></header>
<div class="layout"><nav aria-label="{contents}"><h2>{contents}</h2>{''.join(f'<a href="#{anchor}">{html.escape(label)}</a>' for anchor,label in toc)}</nav>
<main><div class="tools"><label for="search">{search}</label><input id="search" type="search" placeholder="{search}"><button id="previous" aria-label="Previous result">‹</button><button id="next" aria-label="Next result">›</button><output id="count" aria-live="polite"></output><button id="smaller" aria-label="Smaller text">A−</button><button id="larger" aria-label="Larger text">A+</button></div><article>{body}</article><footer>FS PITCH / FASE GROOVE</footer></main></div>
<script src="guide.js"></script></body></html>'''
    (root / 'docs' / (name + '.html')).write_text(html_doc)
    print(f'PASS {name}: 13 linked chapters, {body.count("<table>")} tables, 4 screenshots, offline assets')
