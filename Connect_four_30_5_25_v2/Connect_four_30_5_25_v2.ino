#include <AccelStepper.h>
#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define ROWS 6
#define COLS 7
#define TOTAL_CELLS (ROWS * COLS)

// Define step constant
#define MotorInterfaceType 4

//aproximaçoes para stepping do motor
#define COL1 95
#define COL2 116
#define COL3 137
#define COL4 158
#define COL5 179
#define COL6 200
#define COL7 221


bool gameEnded = false;                // flag para detecao de vitoria, serva para evitar que o microcontrolador empanque na verificaçao
volatile bool resetRequested = false;  //sinal para reset

typedef struct {
  int position;
  short int state;  // 0 = Empty, 1 = Player, 2 = Bot
} Cell;

typedef struct {
  Cell board[TOTAL_CELLS];
} Connect4;

Connect4 game;
//hardware, servomotor, LCD e stepper
const int stepsPerRevolution = 64;  //
Servo myServo;
AccelStepper AssembStepper(MotorInterfaceType, 9, 10, 11, 12);
LiquidCrystal_I2C lcd(0x27, 16, 2);


//Funçao: Wrapper do output serial para que seja possivel transmitir info para o lcd
//a saida vai para serial output e para o ecra ao mesmo tempo,
// logo nao e preciso chamar serial print
void displayMessage(const String& message, bool clearFirst = true) {
  Serial.println(message);
  if (clearFirst) lcd.clear();
  lcd.setCursor(0, 0);

  // se a mensagem for maior que 16 carateres, separar em duas linhas
  if (message.length() <= 16) {
    lcd.print(message);
  } else {
    lcd.print(message.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(message.substring(16, 32));
  }
}



//initializacao, o objetivo : zerar todas a celulas
void init_board(Connect4* game) {
  for (int i = 0; i < TOTAL_CELLS; ++i) {
    game->board[i].position = i;
    game->board[i].state = 0;
  }
}


//colocar uma peca
int drop_disc(Connect4* game, int player, int col) {
  if (col < 0 || col >= COLS) return -1;       // se a coluna for um valor valido
  for (int row = ROWS - 1; row >= 0; row--) {  //este loop serva para assegurar que a peca fica na posicao mais baixa que estiver livre
    int idx = row * COLS + col;
    if (game->board[idx].state == 0) {
      game->board[idx].state = player;
      return idx;  //retorna a posicao onde a peca foi colocada
    }
  }
  return -1;  // Coluna cheia
}

//retornar o estado de uma celula, util se for necessario a outras funcionalidade  lererm o estdo da matriz ie: um bot
int get_cell_state(Connect4* game, int pos) {
  if (pos < 0 || pos >= TOTAL_CELLS) return -1;
  return game->board[pos].state;
}
//funçao: a coluna esta livre se o indice to topo for menor que o maximo del inhas
bool existe_coluna_livre(Connect4* game, int col) {
  if (col < 0 || col >= COLS) return false;
  int topIndex = 0 * COLS + col;
  return game->board[topIndex].state == 0;
}
//funçao: retornar as colunas disponiveis
int get_available_columns(Connect4* game, int* output) {
  int count = 0;
  for (int col = 0; col < COLS; col++) {
    if (existe_coluna_livre(game, col)) {
      output[count] = col;
      count++;
    }
  }
  return count;  // retorna o nr de colunas abertas
}

//funçao: retorna uma representaçao visual da matriz na porta serial
void print_board_state(Connect4* game) {
  Serial.println("Estado atual do tabuleiro:");
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      int idx = r * COLS + c;
      int state = get_cell_state(game, idx);
      if (state == 0) Serial.print(". ");
      else if (state == 1) Serial.print("X ");
      else if (state == 2) Serial.print("O ");
    }
    Serial.println();
  }
  Serial.println();
}

//verificar se alguem ganhou
bool check_winner(Connect4* game, int player) {
  int directions[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };

  /*
		estes valores representam Deltas das diferentes dirrecoes em que e possivel ganhar
		{0,1} horizontal(a mover para direita, linha mantem, coluna aumenta);
		{1,0} vertical(a mover para baixo, lina aumanta coluna mantem);
		{1,1} diagonal ED(coluna e linha aumentam);
		{1,-1} diagonal DE(linha aumenta, coluna diminui);
	*/

  //verifica todas as celulas para o estado(a quem pertencem)
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      int start = row * COLS + col;
      if (game->board[start].state != player) continue;

      //verificar a direcao atraves do Delta com a celula anterior
      for (int d = 0; d < 4; d++) {
        int dr = directions[d][0];
        int dc = directions[d][1];
        int count = 1;  //a peca que foi colocada conta como #1

        // de seguida percorrer as  3 celulas seguintes em cada direcao
        for (int step = 1; step < 4; step++) {
          int nr = row + dr * step;
          int nc = col + dc * step;

          if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) break;  // validacao dee limites

          int idx = nr * COLS + nc;                       // valor convertido para ser legivel na matriz
          if (game->board[idx].state == player) count++;  // a peca pertenca ao mesmo jogador
          else break;
        }

        if (count == 4) return true;  //vitoria atingida
      }
    }
  }

  return false;  // ninguem ganhou..por agora
}

// Verificacao de possibilidade ganhar
/* 	Esta funcao e opcional
	objectivo aqui e determinar se ainda existem estados onde e possivle ganhar, sem ter de esperar que mais pecas sejam jogadas

*/
bool has_possible_win_line(Connect4* game, int player) {
  int directions[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };  //Deltas


  // percorrer a matriz
  for (int row = 0; row < ROWS; row++) {
    for (int col = 0; col < COLS; col++) {
      for (int d = 0; d < 4; d++) {
        int dr = directions[d][0];
        int dc = directions[d][1];
        int valid_cells = 0;    // uma "celula valida", representa uma celula que pertence ao jogador chamado na funcao ou um celula vazia
        bool conflict = false;  //true se encontrar uma celula que pertenca ao oponente numa das direcoes validas

        // le as 3 celulas seguintes ate chegar
        for (int step = 0; step < 4; step++) {
          int nr = row + dr * step;
          int nc = col + dc * step;
          if (nr < 0 || nr >= ROWS || nc < 0 || nc >= COLS) {  //para evitar sair da matriz
            conflict = true;
            break;
          }
          int idx = nr * COLS + nc;
          int state = game->board[idx].state;

          if (state == 0 || state == player) {  //celula vazia ou pertence ao jogador
            valid_cells++;
          } else {
            conflict = true;  //celula do oponente
            break;
          }
        }

        if (!conflict && valid_cells == 4) return true;  //se nao encontrou conflitos e  encontrousequencias onde e possivel alinhar 4
      }
    }
  }
  return false;  //nao existem estados possiveis de ganhar
}

//detecao de empates usa a funcao acima para ler estados possiveis
bool is_draw(Connect4* game) {
  // A matriz esta cheia?
  bool full = true;
  for (int i = 0; i < TOTAL_CELLS; i++) {
    if (game->board[i].state == 0) {
      full = false;
      break;
    }
  }
  if (full) return true;

  // verifica se ainda existem estados onde um jogador pode ganhar
  if (!has_possible_win_line(game, 1) && !has_possible_win_line(game, 2)) {
    return true;  //a matriz nao tem combinacoes vitoriosas para nenhum dos jogadores
  }

  return false;  //nao e empate, continuar
}

const int analogPin = A0;
const int analogPinLCD_1 = A4;
const int analogPinLCD_2 = A5;

const int resetPin = 8;     //
const int FimCursoEsq = 4;  //



// Definiçoes de escada
struct ButtonThreshold {
  const char* name;
  int expectedADC;
};

ButtonThreshold analogButtons[] = {
  { "Button1_10k", int(1023.0 * 100000.0 / (10000.0 + 100000.0)) },  // ~931
  { "Button2_15k", int(1023.0 * 100000.0 / (15000.0 + 100000.0)) },  // ~872
  { "Button3_22k", int(1023.0 * 100000.0 / (22000.0 + 100000.0)) },  // ~823
  { "Button4_33k", int(1023.0 * 100000.0 / (33000.0 + 100000.0)) },  // ~756
  { "Button5_47k", int(1023.0 * 100000.0 / (47000.0 + 100000.0)) },  // ~685
  { "Button6_68k", int(1023.0 * 100000.0 / (68000.0 + 100000.0)) },  // ~604
  { "Button7_82k", int(1023.0 * 100000.0 / (82000.0 + 100000.0)) }   // ~555
};

const int thresholdMargin = 20;


/*Esta funçao e chamada ao inicio para assegurar a posiçao do motor */
void Init_motor() {
  //initializaçao do motor
  displayMessage("A mover...");  //versao para uso do lcd
  AssembStepper.moveTo(100);
  while (digitalRead(FimCursoEsq) == HIGH) {
    AssembStepper.run();
  }
  //displayMessage("Em posicao, aguardar por input"); //versao para uso do lcd
}

// Funçao que espera pela entrada do jogador
int esperar_por_entrada() {
  int analogValue;
  if (resetRequested) {
    return;
  }
  while (true) {

    analogValue = analogRead(analogPin);
    for (int i = 0; i < sizeof(analogButtons) / sizeof(ButtonThreshold); i++) {
      int expected = analogButtons[i].expectedADC;
      if (abs(analogValue - expected) <= thresholdMargin) {
        Serial.print("Botão detectado: ");
        Serial.println(analogButtons[i].name);

        // Verificar se a coluna está cheia
        if (!existe_coluna_livre(&game, i)) {
          displayMessage("Erro: coluna cheia.");
          delay(300);
          continue;  // coluna inválida, esperar nova entrada
        }

        return i;  // coluna válida, retorna o índice
      }
    }
    delay(50);  // debounce
  }
}

// funçao: prcura saber se algum dos jogadores venceu apos cada jogador
void verificar_resultado() {
  if (check_winner(&game, 1)) {
    displayMessage("Jogador 1 venceu!");
    gameEnded = true;
    return;
  }
  if (check_winner(&game, 2)) {
    displayMessage("Bot venceu!");
    gameEnded = true;
    return;
  }
  if (is_draw(&game)) {
    displayMessage("Empate!");
    gameEnded = true;
    return;
  }
}
//logica intenra do turno do jogador
void turno_jogador() {
  if (gameEnded == false) {
    if (resetRequested) {
      return;
    }
    displayMessage("Em posicao, aguardar por input");  //versao para uso do lcd
    int playerCol = esperar_por_entrada();             // obter coluna valida para drop_disc
    if (resetRequested) {
      return;
    }
    int resultPlayer = drop_disc(&game, 1, playerCol);
    if (resetRequested) {
      return;
    }
    if (resultPlayer == -1) {
      displayMessage("Erro ao jogar. coluna cheia.");
      return;
    }
    if (resetRequested) {
      return;
    }
    print_board_state(&game);
    verificar_resultado();
    if (resetRequested || gameEnded) return;
    delay(1000);
  }
}

//logica intenra do turno do bot
void turno_bot() {
  if (resetRequested) {
    return;
  }
  if (gameEnded == false) {

    //  1: Verifica se ainda existem possibilidades de vitoria
    if (!has_possible_win_line(&game, 1) && !has_possible_win_line(&game, 2)) {
      displayMessage("Empate detectado! Zero jogadas possiveis.");
      return;  // parar loop
    }
    // 2: Obter colunas disponiveis
    int availableCols[COLS];
    int totalAvailable = get_available_columns(&game, availableCols);

    if (totalAvailable == 0) {
      displayMessage("Tabuleiro cheio! Empate.");
      return;
    }
    // 3: Escolher aleatoriamente uma das colunas livres
    int botColumn = availableCols[random(0, totalAvailable)];

    String message = String("Bot escolheu a coluna: " + String(botColumn + 1));
    displayMessage(message);

    // 4: realizar a jogada
    move_to_column(botColumn);  // 	mover para a coluna
    soltar_peca();              // 	soltar a peça com o servo
    voltar_para_inicio();       // 		retornar à posição inicial
    // 5: Jogar
    int dropResult = drop_disc(&game, 2, botColumn);
    if (dropResult == -1) {
      displayMessage("Erro: coluna cheia.");  //esta condiçao em termos praticos na odeve acontecer, se se verificar o jogo deve ser reiniciado
    }
    // debug: imprimir tabuleiro
    print_board_state(&game);
    delay(100);  // evitar loops excessivos
  }
  verificar_resultado();
}

//funçao: mover o motor Dc ate a coluna alvo
void move_to_column(int targetCol) {
  if (targetCol < 0 || targetCol >= COLS) {
    Serial.println("Coluna invalida para mover.");
    return;
  }
  int matchCount = 0;             // variavel de controlo para estabilidade
  const int requiredMatches = 1;  // alvo para determinar que atingiu o ponto desejado de forma definitiva
  bool printMessage = true;

  while (true) {
    if (printMessage) {
      String message = String("Movendo para coluna: " + String(targetCol + 1));
      displayMessage(message);
      printMessage = false;
    }
    if (targetCol == 0) {
      AssembStepper.moveTo(COL1);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    if (targetCol == 1) {
      AssembStepper.moveTo(COL2);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    if (targetCol == 2) {
      AssembStepper.moveTo(COL3);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    if (targetCol == 3) {
      AssembStepper.moveTo(COL4);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    if (targetCol == 4) {
      AssembStepper.moveTo(COL5);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    if (targetCol == 5) {
      AssembStepper.moveTo(COL6);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    if (targetCol == 6) {
      AssembStepper.moveTo(COL7);
      while (AssembStepper.distanceToGo() != 0) {
        AssembStepper.run();
      }
      return;
    }
    delay(20);  // delay para estabilidade
  }
}
//Funçao: mover o servo para posicionar e largar a peça
void soltar_peca() {
  displayMessage("A soltar a peca...");
  myServo.write(90);   // abre o mecanismo
  delay(1000);         // espera que a peça caia
  myServo.write(180);  // empurra a peça
  delay(1000);         // espera que ela esteja estável
}
//retornar a posiçao inicial apos largar a peça
void voltar_para_inicio() {
  displayMessage("A voltar para a posicao inicial...");
  AssembStepper.setMaxSpeed(500);        
  AssembStepper.setAcceleration(200);    
  AssembStepper.moveTo(-10000);   
  
  while (digitalRead(FimCursoEsq) == HIGH) {
    AssembStepper.run();
  }
  AssembStepper.setCurrentPosition(0);
  displayMessage("Posicao inicial atingida.");
}
// PCINT setup for D8 (PORTB0)
void setupResetInterrupt() {
  cli();                    // desativar interrupts
  PCICR |= (1 << PCIE0);    // acionar pcint 08-13
  PCMSK0 |= (1 << PCINT0);  // Pcint D8
  sei();                    // ativar interrupts
}
ISR(PCINT0_vect) {  //D8
  if (digitalRead(resetPin) == LOW) {
    resetRequested = true;
  }
}

//Funçao: reiniciar
void reset_jogo() {
  displayMessage("Reset solicitado. Reiniciar jogo...");
  init_board(&game);         // limpar matriz
  Init_motor();              // voltar ao início físico
  myServo.write(180);        //assegurar que o servo esta no posiçao inicial
  print_board_state(&game);  // opcional
  gameEnded = false;
  delay(20);
  //turno_jogador();// o jogador começa sempre primeiro
}

void check_reset() {
  if (resetRequested) {  //tentativa de interrupt simples
    resetRequested = false;
    reset_jogo();
  }
}

void setup() {

  //initializaçao do display
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Arranque pronto");
  //inicializaçao do servo
  myServo.attach(3);
  myServo.write(180);  // pos inicial em baixo
  //stepper
  AssembStepper.setMaxSpeed(1000.0);
  AssembStepper.setAcceleration(50.0);
  AssembStepper.setSpeed(200);
  AssembStepper.moveTo(0);

  voltar_para_inicio();
  //porta serial para debug
  Serial.begin(9600);
  //aquisiçao de pinos
  pinMode(FimCursoEsq, INPUT_PULLUP);
  pinMode(resetPin, INPUT_PULLUP);
  setupResetInterrupt();       // PCINT
  randomSeed(analogRead(A1));  // seed para selecçao aleatoria

  Init_motor();       // inicializar motor
  init_board(&game);  //inicializar matriz
}

void loop() {
  /*sequencia
		- Esperar por entrada
		- Ler estado da mesa
		- Determinar se exitem jogadas possiveis
		- procurar colunas livres
		- Selecionar coluna
		- mover motor ate atingir a coluna pretendida
		- acionar servo
		- esperar que a peça entra em posiçao
		- acionar servo(sentido oposto) para empurrar peça
		- aguardar que a peça caia
		- mover esq ate fim de curso
		- ler mesa
		- detetar vencedor?
	*/
  // Verificar se reset foi pressionado
  if (gameEnded) {
    reset_jogo();
    gameEnded = false;
    return;  // Se o jogo terminou, aguardar reset
  }
  // Lógica do jogo continua apenas se o jogo não acabou
  turno_jogador();  // o jogador começa sempre primeiro e e referencia do como P1
  turno_bot();      //logica interna do turno do bot explicada  na sequencia acima(P2);
  check_reset();
  verificar_resultado();  // verificaçao de segurança para o estado do jogo
}
