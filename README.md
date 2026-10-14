# Melody Code

Plugin da Audio Makers que gera **sub-bass e melodia** no tom escolhido. É o irmão do Chord Code: usa o mesmo motor de harmonia (progressões por estilo e vibe, anti-repetição), escreve uma linha de sub em cima dos acordes e compõe a melodia por cima dos dois.

Formatos: **Standalone** (Windows `.exe` portátil, sem instalação), **VST3** e **AU** (macOS).

### Instalar o VST3 no Windows

Descompacte `Release/Windows/Melody Code 1.1.0 VST3.zip` e copie a pasta `Melody Code.vst3` para `C:\Program Files\Common Files\VST3`. Depois peça para a DAW reescanear os plugins. Com a DAW tocando, o Melody Code segue o tempo e o compasso dela, e manda a melodia (canal 1) e o sub (canal 2) como MIDI para gravar ou tocar outro instrumento.

## Como funciona o motor

1. **Acordes** — `ccp::ProgressionGenerator` (o mesmo código do Chord Code, em `Source/Core/Music.*`) gera um acorde por compasso a partir de Tom, Escala, Style e Vibe. Dá pra trocar o acorde de qualquer compasso pelo seletor no topo do piano roll.
2. **Style e Vibe PADRÃO** — deixando em PADRÃO, o motor escolhe o estilo e a vibe que achar melhor a cada GERAR (o topo mostra a escolha, ex. `PADRÃO → JAZZ`). Estilos: Funk BR, Pop, Trap, Phonk, Drill, Bossa Nova, Jazz, R&B, Reggaeton, House e Lo-fi. A Vibe (Dark, Happy, Sad, Hypnotic, Epic, Chill, Romantic) não mexe nas notas: muda o caráter do som (brilho, reverb, delay, detune, saturação, camada de oitava, tape wobble).
   Acordes simples (tríades e sétimas comuns) com duração variada por estilo: acorde segurado por 2 compassos, troca atrasada (6+2), antecipação de 1/16. Sons: KEYS, BELLS, PAD, PLUCK, SYNTH, MALLET.
3. **Sub** — cada Style tem padrões de sub (tamborzão no Funk BR, 808 com glide no Trap, baião no Piseiro, off-beat no House…). O sub toca a fundamental de cada acorde (E1–D#2), com oitava, quinta e nota de aproximação para o próximo acorde.
4. **Melodia** — forma de frase do estilo (AABA, ABAC, AAAB…): o motivo volta em sequência sobre o novo acorde, a seção C sobe para o clímax, o meio da frase descansa na 3ª/5ª e o fim resolve na tônica. Notas do acorde nos tempos fortes, passagem nos fracos, salto seguido de passo contrário. Cada estilo tem um perfil (células rítmicas com tercinas, swing de colcheia no jazz e de semicolcheia no funk/lo-fi, antecipações, staccato, aproximação cromática, glide). O motor escreve 32 candidatas, pontua (consonância nos tempos fortes, sensível resolvendo, um só clímax, extensão, final na tônica) e fica com a melhor que não tenha saído recentemente.
5. **Notas sobrepostas** — quando soa bem e combina com o estilo: segunda voz em terças (sertanejo, piseiro, pagode), sextas (R&B), shell voicings (jazz, bossa, lo-fi), oitavas (phonk, house, funk), nota pedal (trap, drill), notas ligadas que soam por cima da próxima só em intervalos consonantes, e apojaturas (pagode, jazz, R&B).

## Tela

- **Tom, Escala, Style, Vibe, Sound** no topo, e a **engrenagem** abre as configurações (sincronizar com a DAW, BPM, enviar MIDI).
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
cmake --build build-win --target MelodyCode_Standalone MelodyCode_VST3
```

`scripts/patch-juce-mingw.cmake` ajusta dois detalhes do JUCE para esse alvo (é aplicado automaticamente). O passo pós-build do VST3 chama o `attrib` do Windows; no Linux basta um script vazio chamado `attrib` no PATH. O `.exe` sai com tudo linkado estaticamente, então roda sem instalar nada no Windows 10/11.

O GitHub Actions (`.github/workflows/build.yml`) compila Windows (MSVC) e macOS a cada push e publica os binários como artefato.
