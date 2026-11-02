# Configuração e recuperação por QR code

A tela usa o mesmo desenho nos dois modos e a identidade existente do Pomodoro: mascote tomate, fonte em pixels, coral e cartão do kit visual. Estado da conexão, marca e endereço ficam no topo; QR à esquerda; instrução e senha à direita.

1. Sem celular conectado, o QR contém a rede temporária e sua senha real: `SmallTV-Pomodoro` na configuração ou `SmallTV-Recuperacao` na recuperação.
2. Quando um cliente entra no ponto de acesso, a tela troca para o QR do endereço local do próprio ponto de acesso. O portal cativo pode abrir automaticamente na configuração; se não abrir, esse segundo QR abre o endereço no navegador. A recuperação não tem portal cativo: o segundo QR é o caminho para o painel de recuperação.
3. Se todos os clientes desconectarem, volta ao QR da rede. A tela só é redesenhada quando essa etapa muda.

O QR não contém a senha do Wi-Fi de casa. Essa senha continua sendo informada no formulário de configuração. SSID, senha temporária e endereço local permanecem disponíveis para conexão manual nas respectivas etapas. Os ajustes de cor quente e LCD verde são suspensos durante essas telas para manter o QR preto e branco; os valores salvos não mudam.

A cor verde agora acompanha a tela efetivamente desenhada, em vez do mostrador salvo. Ao entrar em Pomodoro, JPG/GIF, mensagens ou testes da tela, a cor normal ou quente noturna é restaurada antes do primeiro pixel. O verde volta ao retornar a um LCD, se estiver habilitado, e permanece estável na troca entre os dois LCDs. As duas etapas de QR mantêm gama neutro sem reaplicar a cor quente entre elas.

## Geração e limites

- Geração local com Project Nayuki 1.8.0 (MIT), sem servidor ou pedido externo.
- Versões de QR 1 a 4, correção de erro média ou superior, módulos de 3×3 pixels e margem branca de quatro módulos. O maior código ocupa 123×123 pixels na tela de 240×240.
- Dois buffers temporários de 138 bytes; sem framebuffer de tela inteira. SSID e senha escapam `\\`, `;`, `:`, `,` e aspas conforme o formato Wi-Fi do ZXing. Uma credencial não é truncada para caber.
- As redes e senhas de oito caracteres do firmware cabem no código. Se um texto futuro ultrapassar o limite, a tela oferece os dados manuais em vez de mostrar um QR incompleto.

## Verificação

`sim/build.sh` gera prévias de ambas as etapas, nos dois modos e nos três idiomas, e verifica escape e limites do payload. `sim/qr_decode_test.py` lê os arquivos RGB565 gerados pelo firmware com um decodificador independente (dependências de teste: Pillow e zxing-cpp):

```sh
python sim/qr_decode_test.py
```

Os códigos das 13 prévias foram decodificados na resolução nativa de 240×240, incluindo credenciais com caracteres reservados. Compilação 4M2M, testes de segurança/histórico/cor quente e 2.000 casos de fuzzing por formato de GIF/JPEG passaram.

A regressão de cor usa as funções reais do firmware e as telas do simulador, verificando o estado do gama antes de desenhar cada pixel. Inclui LCDs, Pomodoro, JPG/GIF, mensagens, erros, cor/VCOM, prévia, dia/noite, opção verde ligada/desligada e as duas etapas de QR nos modos de configuração e recuperação. A candidata foi validada também pelo simulador Linux de 32 bits.

Ainda falta validar a câmera de Android/iPhone na tela física, a confirmação da entrada na rede sem internet e a troca automática de etapa ao conectar/desconectar. A abertura automática do portal depende do sistema do celular. A versão candidata não foi instalada no relógio.
