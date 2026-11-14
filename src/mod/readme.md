# WholeUI

WholeUI gives Windows 11 one coherent, fluid interface in a single mod, instead of a stack of
separate mods that each patch part of the screen.

**This version changes no window.** It contains the foundation the visual features will be
built on: the compatibility list that decides which windows WholeUI may touch, and a
diagnostic mode to identify any window precisely. Every window still looks exactly as Windows
draws it.

## Compatibility levels

Every window gets one of three levels:

- `native`: WholeUI never touches it. Unknown windows are `native`.
- `visual`: appearance only.
- `full`: dark mode, backdrop, and geometry where WholeUI draws.

The built-in list covers windows the maintainer has validated one by one.

## Diagnostic mode

Turn on **Diagnostic mode** in the settings to see, next to the cursor, the process, module,
window class, dialog template and level of the window under it. Press **Ctrl+Alt+Shift+C**
to copy that window as a ready override line and a compatibility-list line, for example to
report a broken window.

The shortcut is observed, not captured: the app under the cursor also receives
Ctrl+Alt+Shift+C.
While a window running as administrator has the keyboard focus, Windows hides the keys from
WholeUI and nothing is copied. To copy such a window, click the taskbar first, then point at
the window and press the shortcut.

## Overrides

**Compatibility overrides** takes one window per line:

```text
process=explorer.exe; class=CabinetWClass; level=visual
module=sysdm.cpl; level=native
module=shell32.dll; class=#32770; template=1040; level=full
```

Fields: `process`, `module`, `class`, `template` (at least one) and `level` (`native`,
`visual` or `full`). Invalid lines are ignored and logged. Shell surfaces such as the taskbar
and the Start menu cannot be overridden.

## License

GPL-3.0-only. Source: <https://github.com/Onutirachi/WholeUI>

---

## Português (Brasil)

O WholeUI dá ao Windows 11 uma interface coerente e fluida em um único mod, em vez de vários
mods separados que corrigem cada um uma parte da tela.

**Esta versão não altera nenhuma janela.** Ela contém a base sobre a qual os recursos visuais
serão construídos: a lista de compatibilidade, que decide quais janelas o WholeUI pode tocar,
e um modo de diagnóstico para identificar qualquer janela com precisão. Todas as janelas
continuam exatamente como o Windows as desenha.

### Níveis de compatibilidade

Cada janela recebe um de três níveis:

- `native`: o WholeUI nunca a toca. Janelas desconhecidas são `native`.
- `visual`: apenas a aparência.
- `full`: modo escuro, fundo (backdrop) e geometria onde o WholeUI desenha.

A lista embutida cobre as janelas que o mantenedor validou uma a uma.

### Modo de diagnóstico

Ative **Modo de diagnóstico** nas configurações para ver, ao lado do cursor, o processo, o
módulo, a classe da janela, o modelo de diálogo e o nível da janela sob ele. Pressione
**Ctrl+Alt+Shift+C** para copiar essa janela como uma linha de exceção pronta e uma linha da
lista de compatibilidade, por exemplo para relatar uma janela com defeito.

O atalho é observado, não capturado: o aplicativo sob o cursor também recebe
Ctrl+Alt+Shift+C.
Enquanto uma janela executada como administrador tem o foco do teclado, o Windows esconde as
teclas do WholeUI e nada é copiado. Para copiar essa janela, clique primeiro na barra de
tarefas, depois aponte para a janela e pressione o atalho.

### Exceções

**Exceções de compatibilidade** recebe uma janela por linha:

```text
process=explorer.exe; class=CabinetWClass; level=visual
module=sysdm.cpl; level=native
module=shell32.dll; class=#32770; template=1040; level=full
```

Campos: `process`, `module`, `class`, `template` (pelo menos um) e `level` (`native`,
`visual` ou `full`). Linhas inválidas são ignoradas e registradas no log. Superfícies do
shell, como a barra de tarefas e o menu Iniciar, não podem ser alteradas por exceções.

### Licença

GPL-3.0-only. Código-fonte: <https://github.com/Onutirachi/WholeUI>
