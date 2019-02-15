#include <DHT.h>
#include "ACS712.h"
#include <Adafruit_Sensor.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "Arcondicionado.hpp"
#include "Pinos.h"
#include "Conect.h"


WiFiClient espClient;
PubSubClient MQTT(espClient);
float temperatura, umidade;
bool releStatus, presenceStatus, lightStatus;

ACS712 sensor(ACS712_30A, SENSOR_CORRENTE);

#define CHAR_BUFFER_SIZE  18
String sMAC;
char charBufferMAC[CHAR_BUFFER_SIZE];

DHT dht(DHT_DATA_PIN, DHTTYPE);
//Escopo de funções
void testeGeral();
void configWifi();
void reconectWiFi();
void configMQTT();
void reconnectMQTT();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void VerificaConexoesWiFIEMQTT(void);
void EnviaEstadoOutputMQTT(void);

void setup() {

    Serial.begin(115200);
    pinMode(PINO_RELE, OUTPUT);
    pinMode(SENSOR_CORRENTE, INPUT);
    pinMode(PINO_DETECTA_LUZ, INPUT);
    pinMode(PINO_PRESENCA, INPUT);
    pinMode(PINO_STATUS, OUTPUT);
    digitalWrite(PINO_PRESENCA, LOW);
    digitalWrite(PINO_STATUS, LOW);
    configWifi();
    configMQTT();
    sensor.calibrate();
    Arcondicionado_inicializar();

    sMAC = WiFi.macAddress();
    sMAC.toCharArray(charBufferMAC, CHAR_BUFFER_SIZE);

    strcpy(ID_MQTT, charBufferMAC);

    strcpy(TOPICO_SUBSCRIBE_CONTROLE_LUZ, charBufferMAC);
    strcat(TOPICO_SUBSCRIBE_CONTROLE_LUZ, TOPICO_CONTROLE_LUZ);

    strcpy(TOPICO_SUBSCRIBE_CONTROLE_AR, charBufferMAC);
    strcat(TOPICO_SUBSCRIBE_CONTROLE_AR, TOPICO_CONTROLE_AR);

    strcpy(TOPICO_PUBLISH_TEMPERATURA, charBufferMAC);
    strcat(TOPICO_PUBLISH_TEMPERATURA, TOPICO_TEMPERATURA);

    strcpy(TOPICO_PUBLISH_UMIDADE, charBufferMAC);
    strcat(TOPICO_PUBLISH_UMIDADE, TOPICO_UMIDADE);

    strcpy(TOPICO_PUBLISH_LIGHT_STATUS, charBufferMAC);
    strcat(TOPICO_PUBLISH_LIGHT_STATUS, TOPICO_LIGHT_STATUS);

    strcpy(TOPICO_PUBLISH_PRESENCE_STATUS, charBufferMAC);
    strcat(TOPICO_PUBLISH_PRESENCE_STATUS, TOPICO_PRESENCE_STATUS);

    strcpy(TOPICO_PUBLISH_CURRENT_MEASURE, charBufferMAC);
    strcat(TOPICO_PUBLISH_CURRENT_MEASURE, TOPICO_CURRENT_MEASURE);


    Serial.println(String("ID: ") + ID_MQTT);
    Serial.println(TOPICO_SUBSCRIBE_CONTROLE_LUZ);
    Serial.println(TOPICO_SUBSCRIBE_CONTROLE_AR);
    Serial.println(TOPICO_PUBLISH_TEMPERATURA);
    Serial.println(TOPICO_PUBLISH_UMIDADE);
    Serial.println(TOPICO_PUBLISH_LIGHT_STATUS);
    Serial.println(TOPICO_PUBLISH_PRESENCE_STATUS);
    Serial.println(TOPICO_PUBLISH_CURRENT_MEASURE);

}

void loop()
{
  testeGeral();
  VerificaConexoesWiFIEMQTT(); //garante funcionamento das conexões WiFi e ao broker MQTT
  EnviaEstadoOutputMQTT();     //envia o status de todos os outputs para o Broker no protocolo esperado
  delay(1000);
  MQTT.loop();                 //keep-alive da comunicação com broker MQTT
}


////////////////Definições e parâmetros das funções

void testeGeral() //Acionamento a partir do Monitor Serial
{
  if (Serial.available())
  {
    char rcv = Serial.read();
    if (rcv == '1') //Comando que liga ar condicionado em 23 graus
    {
      Serial.println("Ligar ar condicionado em 23 graus");
      Arcondicionado_ligar();
    }
    if (rcv == '2') //Comando que desliga ar condicionado
    {
      Serial.println("Desligar ar condicionado");
      Arcondicionado_desligar();
    }
    if (rcv == '3') //Comando que liga as luzes
    {
      Serial.println("Ligando luzes");
      digitalWrite(PINO_RELE, LOW);
      delay(50);
      if(digitalRead(PINO_DETECTA_LUZ) == 0)
      {
          Serial.println("Luzes ligadas!");
      }
    }
    if (rcv == '4') //Comando que desliga as luzes
    {
      Serial.println("Desligando luzes");
      digitalWrite(PINO_RELE, releStatus);
      delay(50);
      releStatus = !releStatus;
    }
    if (rcv == '5') //Comando lê DHT11
    {
      Serial.println("Lendo temperatura e umidade...");
      umidade = dht.readHumidity();
      temperatura = dht.readTemperature();

      if (isnan(umidade) || isnan(temperatura))
      {
      Serial.println("Erro ao ler o sensor!");
      return;
      }

      Serial.print("Temperatura: ");
      Serial.print(temperatura);
      Serial.print(" Umidade: ");
      Serial.println(umidade);

    }
    if (rcv == '6') //Comando teste de presença
    {
        if(digitalRead(PINO_PRESENCA) == 1)
        {
            Serial.println("Sem presença!");
            presenceStatus = 0;
        }
        if(digitalRead(PINO_PRESENCA) == 0)
        {
            Serial.println("Presença detectada!");
            presenceStatus = 1;
        }
    }
    if (rcv == '7') //Comando teste dos status
    {
        Serial.print("Estado da luz: ");
        Serial.println(lightStatus);
        Serial.print("Estado do relé: ");
        Serial.println(releStatus);
    }
  }
}

void configWifi()
{
    delay(10);
    Serial.print("Conectando à rede Wifi: ");
    Serial.println(SSID);
    Serial.println("Aguarde");
    reconectWiFi();
}

void reconectWiFi()
{
    //se já está conectado a rede WI-FI, nada é feito.
    //Caso contrário, são efetuadas tentativas de conexão
    if (WiFi.status() == WL_CONNECTED){
        return;
    }


    WiFi.begin(SSID, PASS); // Conecta na rede WI-FI

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(100);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Conectado com sucesso na rede ");
    Serial.println(SSID);
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.print("Endereço MAC: ");
    Serial.println(WiFi.macAddress());


}

void configMQTT()
{
    MQTT.setServer(BROKER_MQTT, BROKER_PORT);
    MQTT.setCallback(mqtt_callback);
}

void reconnectMQTT()
{
    while (!MQTT.connected())
    {
        digitalWrite(PINO_STATUS, LOW);
        Serial.print("* Tentando se conectar ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT))
        {
            Serial.println("Conectado com sucesso ao broker MQTT!");
            digitalWrite(PINO_STATUS, HIGH);
            //TOPICO_SUBSCRIBE_CONTROLE_AR.toCharArray(charBuffer, CHAR_BUFFER_SIZE);
            //MQTT.subscribe(charBuffer);
            MQTT.subscribe(TOPICO_SUBSCRIBE_CONTROLE_LUZ);
            //TOPICO_SUBSCRIBE_CONTROLE_AR.toCharArray(charBuffer, CHAR_BUFFER_SIZE);
            //MQTT.subscribe(charBuffer);
            MQTT.subscribe(TOPICO_SUBSCRIBE_CONTROLE_AR);
        }
        else
        {
            Serial.println("Falha ao reconectar no broker.");
            Serial.println("Havera nova tentatica de conexao em 2s");
            delay(2000);
        }
    }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length)
{
    String msg;

    if (strcmp(topic, TOPICO_SUBSCRIBE_CONTROLE_LUZ) == 0)
    {
      Serial.print("Mensagem recebida no tópico: ");
      Serial.println(topic);

      Serial.print("Mensagem:");
      for (unsigned int i = 0 ; i < length ; i++)
      {
        Serial.println((char)payload[i]);
        char c = (char)payload[i];
        msg += c;
      }

      if (msg.equals("1") /* && (lightStatus == 0)*/)
      {
        Serial.println("Desligando luzes(COMUTANDO RELÉ)!");
        digitalWrite(PINO_RELE, releStatus);
        delay(50);
        releStatus = !releStatus;
      }
    }

    if (strcmp(topic, TOPICO_SUBSCRIBE_CONTROLE_AR) == 0)
    {
      Serial.print("Mensagem recebida no tópico: ");
      Serial.println(topic);

      Serial.print("Mensagem:");
      for (unsigned int i = 0 ; i < length ; i++)
      {
        Serial.println((char)payload[i]);
        char c = (char)payload[i];
        msg += c;
      }

      if (msg.equals("0"))
      {
        Serial.println("Desligando ar condicionado!");
        Arcondicionado_desligar();
      }
      if (msg.equals("1"))
      {
        Serial.println("Ligando ar condicionado!");
        Arcondicionado_ligar();
      }
    }

}

void VerificaConexoesWiFIEMQTT(void)
{
    if (!MQTT.connected())
        reconnectMQTT(); //se não há conexão com o Broker, a conexão é refeita

     reconectWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

void EnviaEstadoOutputMQTT(void)
{
  lightStatus = digitalRead(PINO_DETECTA_LUZ);
  presenceStatus = digitalRead(PINO_PRESENCA);

  //float U = 220;
  float I = sensor.getCurrentAC(60);

  Serial.println(String("I = ") + I + " A");

  if (lightStatus == HIGH)
  {
      MQTT.publish(TOPICO_PUBLISH_LIGHT_STATUS, "0");
  }
  if (lightStatus == LOW)
  {
      MQTT.publish(TOPICO_PUBLISH_LIGHT_STATUS, "1");
  }
  if (presenceStatus == 0)
  {
    MQTT.publish(TOPICO_PUBLISH_PRESENCE_STATUS, "0");
  }
  if (presenceStatus == 1)
  {
    MQTT.publish(TOPICO_PUBLISH_PRESENCE_STATUS, "1");
  }
  umidade = dht.readHumidity();
  temperatura = dht.readTemperature();
  //dtostrf(temperatura, 0, 0, charBuffer);
  if(!isnan(temperatura))
  {
    MQTT.publish(TOPICO_PUBLISH_TEMPERATURA, String(temperatura).c_str());
  }
  //dtostrf();
  if(!isnan(umidade))
  {
    MQTT.publish(TOPICO_PUBLISH_UMIDADE, String(umidade).c_str());
  }

    // Medidor de corrente ACS712
    MQTT.publish(TOPICO_PUBLISH_CURRENT_MEASURE, String(I).c_str());

}
