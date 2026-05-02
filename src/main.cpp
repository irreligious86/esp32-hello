#include <Arduino.h>

// Создаём очередь на 5 элементов типа int
QueueHandle_t queue = NULL;

void SenderTask(void* param) {
  int value = 0;
  for(;;) {
    value++;
    // Кладём значение в очередь
    // pdTRUE если успешно
    if (xQueueSend(queue, &value, portMAX_DELAY) == pdTRUE) {
      Serial.printf("Sender: отправил %d\n", value);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void ReceiverTask(void* param) {
  int received = 0;
  for(;;) {
    // Ждём данные из очереди
    if (xQueueReceive(queue, &received, portMAX_DELAY) == pdTRUE) {
      Serial.printf("Receiver: получил %d\n", received);
    }
    // нет vTaskDelay — ждём данные через очередь
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  // Создаём очередь на 5 элементов
  queue = xQueueCreate(5, sizeof(int));

  xTaskCreatePinnedToCore(SenderTask,   "Sender",   2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(ReceiverTask, "Receiver", 2048, NULL, 1, NULL, 1);
}

void loop() {
  vTaskDelay(100 / portTICK_PERIOD_MS);
}