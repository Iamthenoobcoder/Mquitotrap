#include <WiFi.h>

// --- WiFi Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- Hardware Pin Definitions ---
const int FAN_PWM_PIN = 14;  // GPIO 14: Connected to MOSFET gate for fan speed control
const int UV_LED_PIN = 13;   // GPIO 13: Connected to UV LED driver
const int TRIG_PIN = 5;      // GPIO 5: Ultrasonic Sensor Trigger
const int ECHO_PIN = 18;     // GPIO 18: Ultrasonic Sensor Echo

// --- PWM Configuration (For DC Fan Control) ---
#define PWM_FREQUENCY 5000  // 5 kHz frequency
#define PWM_RESOLUTION 10   // 10-bit resolution (0-1023)
#define PWM_CHANNEL 0

// Fan Speed Duty Cycles (0 to 1023)
const int FAN_OFF = 0;
const int FAN_LOW = 400;   // ~40% duty cycle for power saving
const int FAN_HIGH = 900;  // ~90% duty cycle for max suction

// --- Sensing Thresholds (Trap Fullness) ---
// Assuming a sticky trap height of 10 cm (100 mm)
const float MAX_DISTANCE_MM = 100.0; // Empty trap distance
const float FILLING_THRESHOLD_MM = 60.0; // If distance is less than 60mm, it's 'FILLING'
const float FULL_THRESHOLD_MM = 30.0;    // If distance is less than 30mm, it's 'FULL'

// --- System State Variables ---
String current_fan_speed = "OFF";
String current_trap_status = "EMPTY";
String current_uv_status = "OFF";

// Time tracking for periodic updates (e.g., every 30 seconds)
unsigned long last_update_time = 0;
const long update_interval = 30000; // 30 seconds

// --- Function Prototypes ---
void initWiFi();
float readUltrasonicDistance();
void controlFan(String speed_mode);
String getTrapStatus(float distance_mm);
void sendDataToFirestore();

void setup() {
    Serial.begin(115200);

    // 1. Initialize Actuator Pins
    pinMode(UV_LED_PIN, OUTPUT);
    digitalWrite(UV_LED_PIN, LOW); // Start with UV off

    // 2. Initialize Sensor Pins
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // 3. Configure PWM for Fan Control
    // Sets up Timer 0 (PWM_CHANNEL 0)
    ledcSetup(PWM_CHANNEL, PWM_FREQUENCY, PWM_RESOLUTION);
    ledcAttachPin(FAN_PWM_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, FAN_OFF); // Start with fan off

    // 4. Initialize WiFi
    initWiFi();

    // Initial state check
    controlFan("LOW");
    digitalWrite(UV_LED_PIN, HIGH);
    current_uv_status = "ON";
}

void loop() {
    // Check if it's time to send an update
    if (millis() - last_update_time >= update_interval) {
        // --- 1. Sense: Read Trap Fullness ---
        float distance = readUltrasonicDistance();
        current_trap_status = getTrapStatus(distance);
        Serial.printf("Trap Distance: %.2f mm, Status: %s\n", distance, current_trap_status.c_str());

        // --- 2. Actuate Logic (Simple Scheduling Example) ---
        // If the trap is full, switch fan off and notify user (via the dashboard alert)
        if (current_trap_status == "FULL") {
            controlFan("OFF");
            digitalWrite(UV_LED_PIN, LOW);
            current_uv_status = "OFF";
        } else if (WiFi.status() == WL_CONNECTED) {
            // Keep fan and UV running in normal operation
            controlFan("HIGH"); 
            digitalWrite(UV_LED_PIN, HIGH);
            current_uv_status = "ON";
        }

        // --- 3. Connect: Send Data to Cloud ---
        if (WiFi.status() == WL_CONNECTED) {
            sendDataToFirestore();
        }

        last_update_time = millis();
    }
}

/**
 * @brief Initializes the Wi-Fi connection.
 */
void initWiFi() {
    Serial.print("Connecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected.");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
}

/**
 * @brief Reads the distance from the Ultrasonic Sensor (HC-SR04).
 * @return Distance in millimeters (mm).
 */
float readUltrasonicDistance() {
    // Clears the trigPin condition
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    // Sets the trigPin HIGH for 10 microseconds
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    // Reads the echoPin, returns the duration of the sound wave travel time
    long duration = pulseIn(ECHO_PIN, HIGH);
    
    // Calculate the distance in cm (Speed of sound = 340 m/s or 0.034 cm/us)
    // Distance = (Duration * Speed of Sound) / 2
    // For mm: Distance = (duration * 0.34) / 2
    float distance_mm = (duration * 0.34) / 2.0; 

    // Return maximum distance if sensor read fails or is out of range
    if (distance_mm == 0 || distance_mm > MAX_DISTANCE_MM) {
        return MAX_DISTANCE_MM;
    }
    return distance_mm;
}

/**
 * @brief Maps the measured distance to a descriptive trap status.
 * @param distance_mm Measured distance in mm.
 * @return String status: "EMPTY", "FILLING", or "FULL".
 */
String getTrapStatus(float distance_mm) {
    if (distance_mm <= FULL_THRESHOLD_MM) {
        return "FULL";
    } else if (distance_mm <= FILLING_THRESHOLD_MM) {
        return "FILLING";
    } else {
        return "EMPTY";
    }
}

/**
 * @brief Controls the DC fan speed using PWM.
 * @param speed_mode A string: "OFF", "LOW", or "HIGH".
 */
void controlFan(String speed_mode) {
    int duty_cycle = FAN_OFF;
    if (speed_mode == "LOW") {
        duty_cycle = FAN_LOW;
    } else if (speed_mode == "HIGH") {
        duty_cycle = FAN_HIGH;
    }
    ledcWrite(PWM_CHANNEL, duty_cycle);
    current_fan_speed = speed_mode;
}

/**
 * @brief Constructs a JSON payload and sends the current status to Firestore.
 * NOTE: This function is a placeholder representing the actual complex HTTP/Firebase API call.
 * In a real project, you would use an HTTP client (e.g., WiFiClientSecure) to send a POST/PATCH
 * request to a Firebase endpoint or a dedicated Cloud Function.
 */
void sendDataToFirestore() {
    Serial.println("--- Preparing Data Payload ---");
    // Example JSON structure to send to Firestore
    String jsonPayload = "{";
    jsonPayload += "\"fan_speed\":\"" + current_fan_speed + "\",";
    jsonPayload += "\"trap_status\":\"" + current_trap_status + "\",";
    jsonPayload += "\"uv_status\":\"" + current_uv_status + "\",";
    jsonPayload += "\"last_updated\":\"" + String(millis()) + "\"";
    jsonPayload += "}";

    Serial.println("Sending JSON Payload:");
    Serial.println(jsonPayload);
    
    // In a real-world scenario, the complex code for HTTP client setup,
    // handling headers, API keys, and error checking would go here.
    // For the ECE project concept, this demonstrates the intended action:
    // Sending a JSON status update to the cloud.

    Serial.println("Data simulated as sent successfully.");
}