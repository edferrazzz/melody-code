# Melody Code

Plugin da Audio Makers que gera **melodia com uma linha grave** no tom escolhido. É o irmão do Chord Code: usa o mesmo motor de harmonia por baixo, mas aqui os acordes são gerados para o loop inteiro (não se escolhe acorde por compasso). A linha grave é tocada pelo mesmo timbre da melodia, uma ou duas oitavas abaixo; não existe sub-bass separado.

Formatos: **Standalone** (Windows `.exe` portátil, sem instalação), **VST3** e **AU** (macOS).

### Instalar no Windows

Rode `Release/Windows/Melody Code 1.3.0 Instalador.exe` (gerado pelo GitHub Actions). Ele instala o Standalone e o VST3 em `C:\Program Files\Common Files`. Com a DAW tocando, o Melody Code segue o tempo dela e manda a melodia (canal 1) e o grave (canal 2) como MIDI.

## Como funciona o motor

1. **Tom e Escala** — escolha os dois ou deixe em PADRÃO: o motor sorteia o tom e escolhe uma escala que combine com o estilo (o topo mostra a escolha, ex. `PADRÃO → F MENOR NATURAL`).
2. **Acordes** — `ccp::ProgressionGenerator` (código do Chord Code) gera tríades e sétimas simples, com duração variada (acorde segurado, troca adiantada ou atrasada).
3. **Grave** — padrões rítmicos por estilo, com a fundamental entre E2 e D#3, oitava, quinta e nota de aproximação.
4. **Melodia como um produtor** — pergunta e resposta: uma célula rítmica forte (a "pergunta") termina aberta; a resposta ecoa o começo e fecha num tom do acorde; a pergunta volta ajustada ao novo acorde e a frase final resolve na tônica. Contorno com um só clímax, notas do acorde nos tempos fortes, pentatônica nos fracos, salto seguido de passo contrário. O motor escreve 40 candidatas, pontua e fica com a melhor que não saiu recentemente.
5. **Feel** — STRAIGHT, SWING ou TRIPLET (células em tercina).
6. **Vibe** (só som): CLEAN, DREAMY (chorus, camada de oitava, reverb longo), DARK (filtrado, saturação leve, reverb escuro), LO-FI TAPE (fita: wow, bit reduction, filtros) e SPACE (delay ping-pong e reverb enorme). Sons: KEYS, BELLS, PAD, PLUCK, LEAD, MALLET. Saída com limitador suave, sem clip.

## Tela

- **Tom, Escala, Style, Vibe, Sound** no topo; a **engrenagem** abre as configurações (sincronizar com a DAW, BPM, enviar MIDI).
- **Piano roll** editável, com os acordes gerados mostrados onde começam e o grave na faixa BASS.
- **Role**, **Feel**, **Density**, **Range**, **Bars** (2, 4, 8), **BPM**, volume do **Bass** e da **Melody**.
- **GERAR**, **NOVA MELODIA** (mantém acordes e grave), **VARIAÇÃO**, **HISTÓRICO**, **ARRASTAR MIDI** e **SALVAR MIDI** (`- Melody.mid` e `- Bass.mid`).

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
cmake --build build-win --target MelodyCode_Standalone MelodyCode_VST3
```

`scripts/patch-juce-mingw.cmake` ajusta dois detalhes do JUCE para esse alvo (é aplicado automaticamente). O passo pós-build do VST3 chama o `attrib` do Windows; no Linux basta um script vazio chamado `attrib` no PATH. O `.exe` sai com tudo linkado estaticamente, então roda sem instalar nada no Windows 10/11.

O GitHub Actions (`.github/workflows/build.yml`) compila Windows (MSVC) e macOS a cada push e publica os binários como artefato.
