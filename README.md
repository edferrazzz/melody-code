# Melody Code

Plugin da Audio Makers que gera **sub-bass e melodia** no tom escolhido. É o irmão do Chord Code: usa o mesmo motor de harmonia (progressões por estilo e vibe, anti-repetição), escreve uma linha de sub em cima dos acordes e compõe a melodia por cima dos dois.

Formatos: **Standalone** (Windows `.exe` portátil, sem instalação), **VST3** e **AU** (macOS).

## Como funciona o motor

1. **Acordes** — `ccp::ProgressionGenerator` (o mesmo código do Chord Code, em `Source/Core/Music.*`) gera um acorde por compasso a partir de Tom, Escala, Style e Vibe. Dá pra trocar o acorde de qualquer compasso pelo seletor no topo do piano roll.
2. **Sub** — cada Style tem padrões de sub (tamborzão no Funk BR, 808 com glide no Trap, baião no Piseiro, off-beat no House…). O sub toca a fundamental de cada acorde (E1–D#2), com oitava, quinta e nota de aproximação para o próximo acorde.
3. **Melodia** — forma de período (motivo, resposta, motivo em sequência, cadência na tônica). Notas do acorde nos tempos fortes, notas de passagem nos fracos, salto seguido de passo contrário, arco de contorno. Cada estilo tem um perfil (repetição, staccato, extensão, arpejo, aproximação cromática, glide). O motor escreve 24 candidatas, pontua cada uma e fica com a melhor que não tenha saído recentemente.

## Tela

- **Tom, Escala, Style, Vibe, Sound** no topo.
- **Piano roll** editável: clique cria nota, arrastar move, puxar a ponta alonga, duplo clique / botão direito / Delete apaga, ↑↓ muda a nota (Shift = oitava). O sub aparece em ghost na faixa SUB.
- **Cadeado por compasso**: trava acorde, sub e melodia daquele compasso no próximo GERAR.
- **Role** (Lead, Hook, Counter), **Density**, **Range**, **Bars** (2, 4, 8), **BPM**, volume do **Sub** e da **Melody**.
- **GERAR** (tudo), **NOVA MELODIA** (mantém acordes e sub), **NOVO SUB**, **VARIAÇÃO**, **HISTÓRICO**.
- **ARRASTAR MIDI** (arrasta melodia + sub direto pra DAW) e **SALVAR MIDI** (dois arquivos: `- Melody.mid` e `- Sub.mid`).

## Compilar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target MelodyCode_Standalone MelodyCode_VST3
ctest --test-dir build -C Release
```

O JUCE 8.0.12 é baixado automaticamente (ou use `-DFETCHCONTENT_SOURCE_DIR_JUCE=/caminho/do/JUCE`).

### Windows a partir do Linux (zig)

```sh
pip install ziglang
export ZIG="$(python3 -c 'import ziglang,os;print(os.path.dirname(ziglang.__file__))')/zig"
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=scripts/zig-windows-toolchain.cmake
cmake --build build-win --target MelodyCode_Standalone
```

`scripts/patch-juce-mingw.cmake` ajusta dois detalhes do JUCE para esse alvo (é aplicado automaticamente). O `.exe` sai com tudo linkado estaticamente, então roda sem instalar nada no Windows 10/11.

O GitHub Actions (`.github/workflows/build.yml`) compila Windows (MSVC) e macOS a cada push e publica os binários como artefato.
