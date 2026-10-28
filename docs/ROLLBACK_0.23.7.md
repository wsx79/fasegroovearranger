# Ripristino 0.23.7

Base precedente: v0.23.5, commit `f04db0b8c3b3842cbc5c23c78bab173b10546493`.

## Verifiche prima della pubblicazione

- Build Release Linux e CTest: 7/7 PASS.
- UI e rendering nativi, hosting VST3 reale, fixture HTTP e interfaccia AI: PASS.
- Audio offline: 13 canzoni complete e 13 preview, MP3 decodificati indipendentemente: PASS.
- MIDI esportati, quattro motori alle revisioni fissate, guide IT/EN, traduzioni e modulo condiviso: PASS.
- Packaging fixture Linux/Windows: PASS. Windows/macOS reali: vedere CI del tag v0.23.7.
- Patch completa e inversa controllate su indice temporaneo: albero base e candidato ricostruiti esattamente.

## App installata

Conservare i progetti e reinstallare la distribuzione v0.23.5. Nessun cambio del formato progetto.

## Sorgenti condivisi

Per preparare il ripristino senza riscrivere la cronologia, partire da un checkout pulito e annullare tutti i commit successivi alla base, inclusi gli aggiustamenti CI:

```sh
git revert --no-commit f04db0b8c3b3842cbc5c23c78bab173b10546493..v0.23.7
```

Controllare il diff, ricompilare e verificare prima di creare il commit di ripristino. Se esistono modifiche successive, risolvere i conflitti preservandole. Non usare reset o push forzati. Commit, push e release del ripristino richiedono un nuovo via libera.

Dettaglio dei 20 interventi: [AUDIT_FIXES_0.23.7.md](AUDIT_FIXES_0.23.7.md).
