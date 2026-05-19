// =============================================================================
// CONFIGURATION (edit these values to customise the installation)
// =============================================================================

static const uint8_t  SPEAKER_PIN   =  9;       // Digital output pin for tone

static const uint8_t  WPM_STATION_1 = 18;       // STATION 1: ~18 WPM
static const uint8_t  WPM_STATION_2 = 16;       // STATION 2: ~16 WPM

static const uint16_t FREQ_STATION_1 = 650;     // STATION 1 sidetone frequency, Hz
static const uint16_t FREQ_STATION_2  = 820;    // STATION 2 sidetone frequency, Hz

static const uint8_t  JITTER_PCT   =  5;        // Timing jitter ±%
static const uint16_t PAUSE_TX_MS  = 1500;      // Pause between turns, ms
static const uint32_t PAUSE_QSO_MS = 8000UL;    // Silence after full QSO, ms

// =============================================================================
// DERIVED TIMING  (do not edit)
// =============================================================================

static const uint16_t DIT_STATION_1= 1200U / WPM_STATION_1;
static const uint16_t DIT_STATION_2 = 1200U / WPM_STATION_2;   

// =============================================================================
// DATA TYPES
// =============================================================================

struct Station {
    const char* callsign;  
    uint16_t    freqHz;  
    uint16_t    ditMs;    
};

struct QSOStep {
    const Station* station;
    const char*    text;
};

// =============================================================================
// STATION DEFINITIONS
// =============================================================================

static const Station STATION_STATION_1 = { "CALL1", FREQ_STATION_1, DIT_STATION_1};
static const Station STATION_STATION_2 = { "CALL2", FREQ_STATION_2, DIT_STATION_2 };

// =============================================================================
// QSO SCENARIO SCRIPT  (played in order)
// =============================================================================

static const QSOStep QSO_SCRIPT[] = {
    { &STATION_STATION_1, " " },
    { &STATION_STATION_2, " " },
    { &STATION_STATION_1, " " },
    { &STATION_STATION_2, " " },
    { &STATION_STATION_1, " " },
    { &STATION_STATION_2, " " },
};

static const uint8_t QSO_STEPS = sizeof(QSO_SCRIPT) / sizeof(QSO_SCRIPT[0]);

// =============================================================================
// MORSE CODE TABLE
// =============================================================================

struct MorseEntry {
    char        ch;
    const char* code;
};

static const MorseEntry MORSE_TABLE[] = {
    { 'A', ".-"    }, { 'B', "-..."  }, { 'C', "-.-."  }, { 'D', "-.."   },
    { 'E', "."     }, { 'F', "..-."  }, { 'G', "--."   }, { 'H', "...."  },
    { 'I', ".."    }, { 'J', ".---"  }, { 'K', "-.-"   }, { 'L', ".-.."  },
    { 'M', "--"    }, { 'N', "-."    }, { 'O', "---"   }, { 'P', ".--."  },
    { 'Q', "--.-"  }, { 'R', ".-."   }, { 'S', "..."   }, { 'T', "-"     },
    { 'U', "..-"   }, { 'V', "...-"  }, { 'W', ".--"   }, { 'X', "-..-"  },
    { 'Y', "-.--"  }, { 'Z', "--.."  }, { '0', "-----" }, { '1', ".----" },
    { '2', "..---" }, { '3', "...--" }, { '4', "....-" }, { '5', "....." },
    { '6', "-...." }, { '7', "--..." }, { '8', "---.." }, { '9', "----." },
    { '/', "-..-." }, { '\0', nullptr }
};

// =============================================================================
// TIMING HELPER
// =============================================================================

static uint16_t applyJitter(uint16_t baseMs) {
    if (baseMs == 0 || JITTER_PCT == 0) return baseMs;
    int16_t variation = (int16_t)((uint32_t)baseMs * JITTER_PCT / 100UL);
    if (variation < 1) variation = 1;
    int16_t delta  = (int16_t)random(-(long)variation, (long)variation + 1L);
    int16_t result = (int16_t)baseMs + delta;
    return (uint16_t)(result < 1 ? 1 : result);
}

// =============================================================================
// TONE GENERATION
// =============================================================================

static void playElement(const Station* s, bool isDah) {
    const uint16_t nominalMs  = isDah ? (uint16_t)(s->ditMs * 3U) : s->ditMs;
    const uint16_t toneDurMs  = applyJitter(nominalMs);

    tone(SPEAKER_PIN, s->freqHz, toneDurMs); 
    delay(toneDurMs);                       
    noTone(SPEAKER_PIN);                  
    delay(applyJitter(s->ditMs));          
}

// =============================================================================
// MORSE ENCODER
// =============================================================================

static const char* getMorseCode(char c) {
    c = (char)toupper((unsigned char)c);
    for (uint8_t i = 0; MORSE_TABLE[i].ch != '\0'; i++) {
        if (MORSE_TABLE[i].ch == c) {
            return MORSE_TABLE[i].code;
        }
    }
    return nullptr;
}

static void sendChar(const Station* s, char c) {
    const char* code = getMorseCode(c);
    if (code == nullptr) return;

    for (uint8_t i = 0; code[i] != '\0'; i++) {
        playElement(s, code[i] == '-');
    }

    delay(applyJitter(s->ditMs * 2U));
}

static void sendMessage(const Station* s, const char* text) {
    bool prevWasWordBoundary = true;  

    for (uint16_t i = 0; text[i] != '\0'; i++) {
        const char c = text[i];

        if (c == ' ') {
            if (!prevWasWordBoundary) {
                delay(applyJitter(s->ditMs * 4U));
                prevWasWordBoundary = true;
            }
        } else {
            sendChar(s, c);
            prevWasWordBoundary = false;
        }
    }
}

// =============================================================================
// ARDUINO ENTRY POINTS
// =============================================================================

void setup() {
    pinMode(SPEAKER_PIN, OUTPUT);
    noTone(SPEAKER_PIN);
    digitalWrite(SPEAKER_PIN, LOW);

    Serial.begin(9600);
    while (!Serial && millis() < 2000); 
    Serial.println(F("CW QSO Simulator"));
    Serial.println();

    randomSeed(analogRead(A0));

    delay(1000);
}

void loop() {
    // Play every step of the QSO script in sequence.
    for (uint8_t step = 0; step < QSO_STEPS; step++) {
        const QSOStep* q = &QSO_SCRIPT[step];
        
        // Log to serial console.
        Serial.print(F("["));
        Serial.print(q->station->callsign);
        Serial.print(F("] "));
        Serial.println(q->text);
        
        sendMessage(q->station, q->text);

        int32_t txPause = (int32_t)PAUSE_TX_MS + random(-200L, 201L);
        if (txPause < 400L) txPause = 400L;  
        delay((uint32_t)txPause);
    }

    Serial.println();
    Serial.println(F("─ QSO END ─"));
    Serial.println();
    delay(PAUSE_QSO_MS);
}
