const int PIN_SENSOR = 33;
const int PIN_BUZZER = 25;
const unsigned long DEBOUNCE_MS = 150;
const unsigned long FLUXO_JANELA_MS = 60000;
const unsigned long BIP_MS = 50;

const int AMOSTRAS_BASELINE = 200;
const float MARGEM_SIGMA = 12.0;
const int LIMIAR_MINIMO_ADC = 16;
const unsigned long JANELA_AMOSTRA_MS = 50;

const bool MODO_DEBUG = true;

int g_contadorGotas = 0;
unsigned long g_ultimaGotaMs = 0;
unsigned long g_intervaloMs = 0;

int g_ultimoContador = 0;
unsigned long g_inicioJanelaMs = 0;
int g_gotasNaJanela = 0;

float g_baseline = 0.0;
float g_ruido = 0.0;
int g_limiar = LIMIAR_MINIMO_ADC;

enum Estado
{
    CALIBRANDO,
    MONITORANDO
};
Estado g_estado = CALIBRANDO;

int g_amostrasCalibracao[200];
int g_idxCalibracao = 0;

int g_picoJanela = 0;
unsigned long g_inicioJanelaAmostra = 0;
unsigned long g_buzzerOffMs = 0;

int lerSensor()
{
    return analogRead(PIN_SENSOR);
}

void calibrar()
{
    int amostra = lerSensor();
    g_amostrasCalibracao[g_idxCalibracao] = amostra;
    g_idxCalibracao++;

    if (g_idxCalibracao % 50 == 0)
    {
        Serial.printf("Calibrando... %d/%d (raw: %d)\n",
                      g_idxCalibracao, AMOSTRAS_BASELINE, amostra);
    }

    if (g_idxCalibracao >= AMOSTRAS_BASELINE)
    {
        float soma = 0.0;
        for (int i = 0; i < AMOSTRAS_BASELINE; i++)
        {
            soma += g_amostrasCalibracao[i];
        }
        g_baseline = soma / (float)AMOSTRAS_BASELINE;

        float somaQuad = 0.0;
        for (int i = 0; i < AMOSTRAS_BASELINE; i++)
        {
            float diff = g_amostrasCalibracao[i] - g_baseline;
            somaQuad += diff * diff;
        }
        g_ruido = sqrtf(somaQuad / (float)AMOSTRAS_BASELINE);

        int limiarCalc = (int)(g_ruido * MARGEM_SIGMA);
        g_limiar = limiarCalc > LIMIAR_MINIMO_ADC ? limiarCalc : LIMIAR_MINIMO_ADC;

        g_estado = MONITORANDO;
        g_inicioJanelaMs = millis();
        g_inicioJanelaAmostra = millis();

        Serial.println();
        Serial.println("=== Calibracao concluida ===");
        Serial.printf("Baseline: %.1f | Ruido: %.2f | Limiar: %d ADC\n",
                      g_baseline, g_ruido, g_limiar);
        Serial.println("Aguardando gotas...\n");
    }
}

void monitorar()
{
    static unsigned long debugTimer = 0;

    int leitura = lerSensor();
    int desvio = abs(leitura - (int)g_baseline);

    if (desvio > g_picoJanela)
    {
        g_picoJanela = desvio;
    }

    unsigned long agora = millis();
    if (agora - g_inicioJanelaAmostra >= JANELA_AMOSTRA_MS)
    {
        int pico = g_picoJanela;
        g_picoJanela = 0;
        g_inicioJanelaAmostra = agora;

        if (MODO_DEBUG)
        {
            Serial.printf("[DBG] pico=%d lim=%d baseline=%.0f\n",
                          pico, g_limiar, g_baseline);
        }

        if (pico >= g_limiar)
        {
            if (agora - g_ultimaGotaMs > DEBOUNCE_MS)
            {
                g_intervaloMs = g_ultimaGotaMs > 0 ? agora - g_ultimaGotaMs : 0;
                g_contadorGotas++;
                g_ultimaGotaMs = agora;

                if (MODO_DEBUG)
                {
                    Serial.printf(">>> DETECTADA: pico=%d\n", pico);
                }

                digitalWrite(PIN_BUZZER, HIGH);
                g_buzzerOffMs = agora + BIP_MS;
            }
        }
    }
}

float calcularFluxoInstantaneo()
{
    if (g_intervaloMs == 0)
        return 0.0;
    return 60000.0 / (float)g_intervaloMs;
}

float calcularFluxoJanela()
{
    unsigned long decorrido = millis() - g_inicioJanelaMs;
    if (decorrido < 1000)
        return 0.0;
    return (float)g_gotasNaJanela / ((float)decorrido / 60000.0);
}

void exibirGota()
{
    float fluxoInst = calcularFluxoInstantaneo();
    float fluxoJanela = calcularFluxoJanela();

    Serial.printf("Gota #%d", g_contadorGotas);
    if (g_intervaloMs > 0)
    {
        Serial.printf(" | Intervalo: %lu ms", g_intervaloMs);
    }
    Serial.printf(" | Fluxo inst: %.1f gotas/min", fluxoInst);
    Serial.printf(" | Fluxo media: %.1f gotas/min", fluxoJanela);
    Serial.println();
}

void recalcularBaseline()
{
    static unsigned long ultimaRecalc = 0;
    unsigned long agora = millis();
    if (agora - ultimaRecalc < 15000)
        return;
    ultimaRecalc = agora;

    float soma = 0.0;
    for (int i = 0; i < 30; i++)
    {
        soma += lerSensor();
        delay(1);
    }
    float novaBaseline = soma / 30.0;
    float diff = abs(novaBaseline - g_baseline);
    if (diff < (float)g_limiar)
    {
        g_baseline = g_baseline * 0.7 + novaBaseline * 0.3;
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    Serial.println();
    Serial.println("==========================================");
    Serial.println("  Contador de Gotas IR v3.3");
    Serial.println("  Pico em janela + Buzzer");
    Serial.println("==========================================");
    Serial.println();
    Serial.println("CALIBRACAO: mantenha o feixe LIVRE (sem gotas)");
    Serial.println();
}

void loop()
{
    switch (g_estado)
    {
    case CALIBRANDO:
        calibrar();
        delay(10);
        break;

    case MONITORANDO:
        monitorar();

        if (g_contadorGotas != g_ultimoContador)
        {
            g_gotasNaJanela += (g_contadorGotas - g_ultimoContador);
            g_ultimoContador = g_contadorGotas;
            exibirGota();
        }

        {
            unsigned long decorridoJanela = millis() - g_inicioJanelaMs;
            if (decorridoJanela >= FLUXO_JANELA_MS)
            {
                float fluxoJanela = calcularFluxoJanela();
                Serial.printf("[Janela %lus] Gotas: %d | Fluxo: %.1f gotas/min\n",
                              decorridoJanela / 1000, g_gotasNaJanela, fluxoJanela);
                g_inicioJanelaMs = millis();
                g_gotasNaJanela = 0;
            }
        }

        recalcularBaseline();

        if (g_buzzerOffMs > 0 && millis() >= g_buzzerOffMs)
        {
            digitalWrite(PIN_BUZZER, LOW);
            g_buzzerOffMs = 0;
        }

        delay(1);
        break;
    }
}
