# Plugin Veyon - InternetControl (Controle de Internet para Salas de Aula)

[![Veyon Version](https://img.shields.io/badge/Veyon-4.10%2B-blue.svg)](https://veyon.io)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%2F%2011%20(64--bit)-brightgreen.svg)](#)
[![License](https://img.shields.io/badge/License-GPLv2-green.svg)](#)

O **InternetControl** é um plugin de alto desempenho para o **Veyon (versão 4.10+)**, desenvolvido sob medida para **laboratórios de informática e salas de aula com ambiente Windows**. 

Ele oferece ao docente o controle total do acesso à web dos alunos diretamente da barra de ferramentas do **Veyon Master**, permitindo alternar instantaneamente entre **Navegação Livre**, **Bloqueio Total (Sem Internet)** ou **Modo Prova (Apenas Sites Autorizados)** com um único clique.

---

## 🎯 Por que este plugin foi criado?

Em ambientes educacionais, os métodos tradicionais de bloqueio de internet frequentemente causam frustração aos professores:
- Alunos utilizavam o **Microsoft Edge** para burlar bloqueios que só afetavam o Chrome.
- Quando o professor ativava o bloqueio, as regras só entravam em vigor **se o aluno fechasse e abrisse o navegador manualmente**.
- Alunos abriam janelas anônimas (*InPrivate / Incognito*) para escapar de restrições.
- Bloqueios de rede convencionais derrubavam a conexão do próprio Veyon e travavam o compartilhamento de tela.

**O InternetControl resolve todos esses problemas de forma definitiva e automática.**

---

## 🚀 Recursos Principais

| Recurso | Descrição |
| :--- | :--- |
| 🛡️ **Dois Modos em Um Botão** | **Bloqueio Total** (desconecta toda a web externa) ou **Modo Whitelist** (libera apenas os sites da aula/prova). |
| 🌐 **Compatibilidade Universal** | Suporte total e rigoroso aos principais navegadores: **Microsoft Edge, Google Chrome, Mozilla Firefox, Brave, Opera e Opera GX**. |
| ⚡ **Atualização Instantânea** | Os navegadores abertos no aluno são atualizados na hora. O professor não precisa pedir para nenhum aluno fechar navegador. |
| 🔄 **Restauração de Abas** | Ao reiniciar o navegador para aplicar o bloqueio ou liberação, as abas do aluno são preservadas (`--restore-last-session`). Nenhum trabalho escolar é perdido. |
| 🔒 **Anti-Burla Rigoroso** | Desativa navegação anônima (*InPrivate*), neutraliza o *Startup Boost* do Edge, desativa protocolos de bypass (QUIC/HTTP3) e bloqueia perfis pessoais não autorizados. |
| 🏫 **Preservação da Rede Local** | A comunicação entre o computador do professor e os alunos (portas 11100 e 11400), o compartilhamento de arquivos na rede da escola e impressoras locais **nunca são afetados**. |
| ⚡ **Desbloqueio Imediato** | Ao clicar em Liberar, a internet é restaurada no aluno em menos de 10 milissegundos. |

---

## 👨‍🏫 Guia de Uso para Professores (Docentes)

Na barra superior do **Veyon Master**, você verá dois botões dedicados ao controle de internet:

```
[ 🔴 Bloquear Internet ]   [ ⚙️ Configurar Sites Liberados ]
```

### 1. Como fazer um Bloqueio Total (Aula Expositiva ou Prova Offline)
Use este modo quando você quiser atenção total dos alunos na explicação ou quando eles estiverem realizando uma atividade sem consulta à internet.

1. Clique no botão **⚙️ Configurar Sites Liberados**.
2. Clique no botão vermelho **🔴 Bloqueio Total (Limpar Lista)** para deixar a caixa de texto em branco.
3. Clique em **OK**.
4. Selecione os computadores dos alunos na grade do Veyon (ou não selecione nenhum para aplicar na sala inteira).
5. Clique no botão **🔴 Bloquear Internet**.
   - *O que acontece no aluno:* Todos os navegadores abertos são reiniciados instantaneamente exibindo aviso de bloqueio. Qualquer tentativa de acessar sites externos falhará imediatamente. A rede local e a sua visão no Veyon continuam funcionando perfeitamente.

---

### 2. Como fazer um Modo Prova / Pesquisa (Apenas Sites Autorizados)
Use este modo para aplicar provas online (Moodle, Google Sala de Aula, formulários) ou aulas de pesquisa dirigida (Wikipédia, Khan Academy).

1. Clique no botão **⚙️ Configurar Sites Liberados**.
2. Digite os sites que os alunos **poderão acessar**, um por linha. Exemplos:
   ```text
   wikipedia.org
   khanacademy.org
   moodle.minhaescola.edu.br
   gov.br
   ```
   *(Você também pode clicar no botão verde **🟢 Modelo de Prova** para preencher rapidamente).*
3. Clique em **OK**.
4. Selecione os computadores desejados e clique no botão **🔴 Bloquear Internet** (o botão ficará pressionado com o texto **Liberar Internet**).
   - *O que acontece no aluno:* O aluno só conseguirá carregar os sites informados na lista. Se ele tentar acessar YouTube, redes sociais, jogos ou qualquer outro site, o navegador mostrará a página de bloqueio administrativo.

> [!TIP]
> **Alteração em Tempo Real:** Se a turma já estiver bloqueada e você perceber que esqueceu de liberar um site (por exemplo, um site de notícias ou dicionário), basta clicar em **⚙️ Configurar Sites Liberados**, adicionar o endereço e clicar em **OK**. O Veyon Master atualizará todos os computadores que já estavam bloqueados automaticamente, sem precisar destravar e travar de novo!

---

### 3. Como Liberar a Internet dos Alunos
Ao final da atividade ou aula:
1. Clique novamente no botão **Liberar Internet** (que desmarcará a opção).
2. A navegação volta ao normal instantaneamente em todos os navegadores abertos.

---

## 📝 Como Cadastrar os Sites no "Configurar Sites Liberados"

Para garantir que o filtro funcione com perfeição, siga estas recomendações simples:

- **Digite apenas o domínio:** Não é necessário colocar `https://` ou `www.`.
  - ✅ **Certo:** `wikipedia.org` *(já libera automaticamente `www.wikipedia.org`, `pt.wikipedia.org`, etc.)*
  - ✅ **Certo:** `moodle.minhaescola.edu.br`
  - ❌ **Evite:** `https://www.wikipedia.org/wiki/Pagina_Principal`
- **Sites com serviços associados (Google Forms / Moodle / Vídeos):**
  - Se você aplicar uma prova via **Google Forms**, além de `docs.google.com`, adicione também `accounts.google.com` (para login) e `gstatic.com` (para carregar imagens e estilos do formulário).
  - Se o Moodle da escola usar login institucional integrado (ex: `login.escola.edu.br`), lembre-se de listar tanto o Moodle quanto o domínio de autenticação.

---

## 💻 Instalação Passo a Passo

### Pré-requisitos
- Sistema Operacional: **Windows 10 ou Windows 11 (64 bits)** em todas as máquinas.
- **Veyon 4.10.x** já instalado tanto no computador do professor quanto nos computadores dos alunos.

---

### A. No Computador do Professor (Veyon Master)

1. Baixe ou copie a pasta deste projeto no computador do professor.
2. Localize o arquivo **`instalar_plugin.bat`**.
3. Clique com o botão direito sobre ele e selecione **"Executar como Administrador"**.
4. O instalador executará os seguintes passos automaticamente:
   - Encerra temporariamente o serviço do Veyon.
   - Instala a biblioteca `internetcontrol.dll` em `C:\Program Files\Veyon\plugins\`.
   - Oculta a tela do computador do próprio professor da grade de monitoramento (para evitar efeito de espelho infinito).
   - Reinicia o serviço do Veyon.
5. Abra o **Veyon Master**. Os botões **Bloquear Internet** e **Configurar Sites Liberados** já estarão disponíveis na barra de ferramentas.

---

### B. Nos Computadores dos Alunos (Veyon Server)

Nos computadores dos alunos só é necessário instalar o arquivo uma única vez:

1. Copie o arquivo **`instalar_aluno.bat`** e o **`internetcontrol.dll`** para o computador do aluno (ou acesse via compartilhamento de rede / pendrive).
2. Clique com o botão direito em **`instalar_aluno.bat`** e selecione **"Executar como Administrador"**.
3. O script instalará o plugin, registrará os módulos e reiniciará o serviço do aluno em segundos.
4. Pronto! O computador do aluno já responderá imediatamente a todos os comandos do professor.

> [!NOTE]
> **Dica para Administradores de TI:** Em laboratórios com muitas máquinas, você pode distribuir o `internetcontrol.dll` copiando-o diretamente para `C:\Program Files\Veyon\plugins\` através de GPO (Diretiva de Grupo), PDQ Deploy, script de inicialização do Windows ou clonagem de imagem.

---

## 🆘 Solução de Dúvidas e Emergências

### ❓ E se faltar energia ou o professor esquecer a internet dos alunos bloqueada?
O plugin possui mecanismos de proteção contra bloqueios travados:
- **Ao reiniciar o PC do aluno:** Se o computador for reiniciado, o plugin detecta automaticamente se não há nenhum Veyon Master conectado ordenando o bloqueio e restaura a internet por segurança.
- **Script de Liberação de Emergência:** Acompanha o projeto o arquivo **`liberar_internet_emergencia.bat`**. Caso precise liberar a internet de um aluno sem usar o Veyon Master, basta executar esse arquivo como Administrador no computador do aluno. Ele limpa todas as regras de firewall e políticas em 2 segundos.

### ❓ O Veyon vai travar ou parar de exibir a tela do aluno quando eu bloquear a internet?
**Não.** O plugin foi projetado com prioridade para manter o Veyon 100% ativo. O bloqueio só atua sobre conexões de saída públicas (portas 80 e 443 para a internet). As redes locais privadas (faixas `192.168.x.x`, `10.x.x.x`, `172.16.x.x`) e as portas do Veyon (11100 e 11400) permanecem totalmente abertas.

### ❓ Se o aluno tiver uma conta pessoal conectada no Microsoft Edge, ele consegue navegar?
**Não.** Versões recentes do Edge (116+) ignoravam regras de bloqueio se houvesse conta pessoal conectada. Nosso plugin detecta automaticamente perfis pessoais no Edge e aplica travas administrativas em tempo de execução para garantir que o bloqueio seja rigorosamente cumprido.

### ❓ Onde ficam os registros (logs) para conferência de TI?
Todos os eventos de bloqueio, liberação e reinicialização de navegadores são gravados no arquivo:
`C:\ProgramData\VeyonInternetControl\internetcontrol.log`

---

## 🛠️ Para Desenvolvedores e Equipe de TI (Compilação)

Caso deseje compilar o plugin a partir do código-fonte:

### Requisitos
- Windows 10/11 (64-bit)
- **MSYS2** (`C:\msys64`) com o ambiente **MINGW64**:
  - `mingw-w64-x86_64-gcc`
  - `mingw-w64-x86_64-qt6-base`
  - `mingw-w64-x86_64-cmake`
  - `mingw-w64-x86_64-ninja`

### Compilando
Abra o PowerShell na pasta do projeto e execute:
```powershell
.\build.ps1
```

A DLL compilada será gerada em `build\internetcontrol.dll`. Para compilar e instalar diretamente na máquina local:
```powershell
.\build.ps1 -Deploy
```

---

## 📄 Licença

Este plugin é distribuído sob a licença GNU General Public License v2 (GPLv2), compatível com a licença do projeto oficial Veyon.
