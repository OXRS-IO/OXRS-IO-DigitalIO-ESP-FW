/**
  ESP32 digital I/O firmware for the Open eXtensible Rack System

  Documentation:  
    https://oxrs.io/docs/firmware/digital-io-esp32.html

  GitHub repository:
    https://github.com/OXRS-IO/OXRS-IO-DigitalIO-ESP32-FW

  Copyright 2019-2023 SuperHouse Automation Pty Ltd
*/

/*--------------------------- Libraries -------------------------------*/
#include <Arduino.h>
#include <OXRS_Input.h>               // For input handling
#include <OXRS_Output.h>              // For output handling

// [BJ] I tried adding ESP8266 support but the adoption payload was too
// [BJ] big and was blowing the stack - would be nice to get that working...
#if defined(OXRS_ESP32)
#include <OXRS_32.h>                  // ESP32 support
OXRS_32 oxrs;
const uint8_t GPIO_PINS[]         = { 2, 4, 5, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27 };
#elif defined(OXRS_LILYGO)
#include <OXRS_LILYGOPOE.h>           // LilyGO T-ETH-POE support
OXRS_LILYGOPOE oxrs;
const uint8_t GPIO_PINS[]         = { 2, 4, 12, 14, 15, 16, 32, 33, 34, 35, 36, 39 };
#endif

/*--------------------------- Constants -------------------------------*/
// Serial
#define       SERIAL_BAUD_RATE      115200

// Internal constant used when GPIO pin parsing fails
#define       INVALID_GPIO_PIN      99

// Internal constant used when GPIO type parsing fails
#define       INVALID_GPIO_TYPE     99

// Internal constant used when input type parsing fails
#define       INVALID_INPUT_TYPE    99

// Internal constants used when output type parsing fails
#define       INVALID_OUTPUT_TYPE   99

/*--------------------------- Global Variables ------------------------*/
enum gpioType_t { GPIO_INPUT, GPIO_OUTPUT };

const uint8_t GPIO_COUNT          = sizeof(GPIO_PINS);
uint8_t gpioTypes[GPIO_COUNT];

/*--------------------------- Instantiate Globals ---------------------*/
// Input handler
OXRS_Input oxrsInput;

// Output handler
OXRS_Output oxrsOutput;

/*--------------------------- Program ---------------------------------*/

// Set the type in our internal config and update the physical pin mode
void setGpioType(uint8_t index, uint8_t type)
{
  // update the GPIO type in our internal config
  gpioTypes[index] = type;

  // get the GPIO pin
  uint8_t gpio = GPIO_PINS[index];

  // configure the GPIO pin itself
  switch (type)
  {
    case GPIO_INPUT:
      pinMode(gpio, INPUT_PULLUP);
      break;

    case GPIO_OUTPUT:
      pinMode(gpio, OUTPUT);
      digitalWrite(gpio, RELAY_OFF);
      break;
  }
}

// Read all input GPIOs at once and make 16-bit result (mimic MCP)
uint16_t readInputs(void)
{
  uint16_t result = 0xffff;
  uint32_t inReg = REG_READ(GPIO_IN_REG);

  for (uint8_t index = 0; index < GPIO_COUNT; index++)
  {
    if (gpioTypes[index] == GPIO_INPUT)
    {
      uint8_t gpio = GPIO_PINS[index];

      if (!bitRead(inReg, gpio)) 
      { 
        bitClear(result, index);
      }
    }
  }
  
  return result;
}

// Convert GPIO pin (from JSON payload) to 0-based index
uint8_t getIndexFromGpio(uint8_t gpio)
{
  for (uint8_t index = 0; index < GPIO_COUNT; index++)
  {
    if (gpio == GPIO_PINS[index])
    {
      return index;
    }
  }

  return INVALID_GPIO_PIN;
}

void createGpioPinEnum(JsonObject parent)
{
  JsonArray pinEnum = parent.createNestedArray("enum");

  for (uint8_t index = 0; index < GPIO_COUNT; index++)
  {
    pinEnum.add(GPIO_PINS[index]);
  }
}

void createGpioTypeEnum(JsonObject parent)
{
  JsonArray typeEnum = parent.createNestedArray("enum");
  
  typeEnum.add("input");
  typeEnum.add("output");
}

JsonObject createGpioTypeDependency(JsonArray parent, uint8_t type)
{
  JsonObject element = parent.createNestedObject();
  JsonObject elementProperties = element.createNestedObject("properties");
  JsonObject elementType = elementProperties.createNestedObject("type");

  JsonArray elementTypeEnum = elementType.createNestedArray("enum");
  switch (type)
  {
    case GPIO_INPUT:
      elementTypeEnum.add("input");
      return elementProperties.createNestedObject("input");

    case GPIO_OUTPUT:
      elementTypeEnum.add("output");
      return elementProperties.createNestedObject("output");

    default:
      // Should never get here
      return elementProperties;
  }
}

uint8_t parseGpioType(const char * gpioType)
{
  if (strcmp(gpioType, "input")   == 0) { return GPIO_INPUT; }
  if (strcmp(gpioType, "output")  == 0) { return GPIO_OUTPUT; }

  oxrs.println(F("[digio] invalid GPIO type"));
  return INVALID_GPIO_TYPE;
}

void createInputTypeEnum(JsonObject parent)
{
  JsonArray typeEnum = parent.createNestedArray("enum");
  
  typeEnum.add("button");
  typeEnum.add("contact");
  typeEnum.add("press");
  typeEnum.add("rotary");
  typeEnum.add("security");
  typeEnum.add("switch");
  typeEnum.add("toggle");
}

uint8_t parseInputType(const char * inputType)
{
  if (strcmp(inputType, "button")   == 0) { return BUTTON; }
  if (strcmp(inputType, "contact")  == 0) { return CONTACT; }
  if (strcmp(inputType, "press")    == 0) { return PRESS; }
  if (strcmp(inputType, "rotary")   == 0) { return ROTARY; }
  if (strcmp(inputType, "security") == 0) { return SECURITY; }
  if (strcmp(inputType, "switch")   == 0) { return SWITCH; }
  if (strcmp(inputType, "toggle")   == 0) { return TOGGLE; }

  oxrs.println(F("[digio] invalid input type"));
  return INVALID_INPUT_TYPE;
}

void getInputType(char inputType[], uint8_t type)
{
  // Determine what type of input we have
  sprintf_P(inputType, PSTR("error"));
  switch (type)
  {
    case BUTTON:
      sprintf_P(inputType, PSTR("button"));
      break;
    case CONTACT:
      sprintf_P(inputType, PSTR("contact"));
      break;
    case PRESS:
      sprintf_P(inputType, PSTR("press"));
      break;
    case ROTARY:
      sprintf_P(inputType, PSTR("rotary"));
      break;
    case SECURITY:
      sprintf_P(inputType, PSTR("security"));
      break;
    case SWITCH:
      sprintf_P(inputType, PSTR("switch"));
      break;
    case TOGGLE:
      sprintf_P(inputType, PSTR("toggle"));
      break;
  }
}

void getInputEventType(char eventType[], uint8_t type, uint8_t state)
{
  // Determine what event we need to publish
  sprintf_P(eventType, PSTR("error"));
  switch (type)
  {
    case BUTTON:
      switch (state)
      {
        case HOLD_EVENT:
          sprintf_P(eventType, PSTR("hold"));
          break;
        case 1:
          sprintf_P(eventType, PSTR("single"));
          break;
        case 2:
          sprintf_P(eventType, PSTR("double"));
          break;
        case 3:
          sprintf_P(eventType, PSTR("triple"));
          break;
        case 4:
          sprintf_P(eventType, PSTR("quad"));
          break;
        case 5:
          sprintf_P(eventType, PSTR("penta"));
          break;
      }
      break;
    case CONTACT:
      switch (state)
      {
        case LOW_EVENT:
          sprintf_P(eventType, PSTR("closed"));
          break;
        case HIGH_EVENT:
          sprintf_P(eventType, PSTR("open"));
          break;
      }
      break;
    case PRESS:
      sprintf_P(eventType, PSTR("press"));
      break;
    case ROTARY:
      switch (state)
      {
        case LOW_EVENT:
          sprintf_P(eventType, PSTR("up"));
          break;
        case HIGH_EVENT:
          sprintf_P(eventType, PSTR("down"));
          break;
      }
      break;
    case SECURITY:
      switch (state)
      {
        case HIGH_EVENT:
          sprintf_P(eventType, PSTR("normal"));
          break;
        case LOW_EVENT:
          sprintf_P(eventType, PSTR("alarm"));
          break;
        case TAMPER_EVENT:
          sprintf_P(eventType, PSTR("tamper"));
          break;
        case SHORT_EVENT:
          sprintf_P(eventType, PSTR("short"));
          break;
        case FAULT_EVENT:
          sprintf_P(eventType, PSTR("fault"));
          break;
      }
      break;
    case SWITCH:
      switch (state)
      {
        case LOW_EVENT:
          sprintf_P(eventType, PSTR("on"));
          break;
        case HIGH_EVENT:
          sprintf_P(eventType, PSTR("off"));
          break;
      }
      break;
    case TOGGLE:
      sprintf_P(eventType, PSTR("toggle"));
      break;
  }
}

void createOutputTypeEnum(JsonObject parent)
{
  JsonArray typeEnum = parent.createNestedArray("enum");

  typeEnum.add("relay");
  typeEnum.add("motor");
  typeEnum.add("timer");
}

uint8_t parseOutputType(const char *outputType)
{
  if (strcmp(outputType, "relay") == 0)
  {
    return RELAY;
  }
  if (strcmp(outputType, "motor") == 0)
  {
    return MOTOR;
  }
  if (strcmp(outputType, "timer") == 0)
  {
    return TIMER;
  }

  oxrs.println(F("[digio] invalid output type"));
  return INVALID_OUTPUT_TYPE;
}

void getOutputType(char outputType[], uint8_t type)
{
  // Determine what type of output we have
  sprintf_P(outputType, PSTR("error"));
  switch (type)
  {
  case MOTOR:
      sprintf_P(outputType, PSTR("motor"));
      break;
  case RELAY:
      sprintf_P(outputType, PSTR("relay"));
      break;
  case TIMER:
      sprintf_P(outputType, PSTR("timer"));
      break;
  }
}

void getOutputEventType(char eventType[], uint8_t type, uint8_t state)
{
  // Determine what event we need to publish
  sprintf_P(eventType, PSTR("error"));
  switch (state)
  {
  case RELAY_ON:
      sprintf_P(eventType, PSTR("on"));
      break;
  case RELAY_OFF:
      sprintf_P(eventType, PSTR("off"));
      break;
  }
}

/**
 Status publishing
*/
void publishInputEvent(uint8_t index, uint8_t type, uint8_t state)
{
  char inputType[9];
  getInputType(inputType, type);
  char eventType[7];
  getInputEventType(eventType, type, state);

  StaticJsonDocument<64> json;
  json["gpio"] = GPIO_PINS[index];
  json["type"] = inputType;
  json["event"] = eventType;

  if (!oxrs.publishStatus(json.as<JsonVariant>()))
  {
    oxrs.print(F("[digio] [failover] "));
    serializeJson(json, oxrs);
    oxrs.println();

    // TODO: add failover handling code here
  }
}

void publishOutputEvent(uint8_t index, uint8_t type, uint8_t state)
{
  char outputType[8];
  getOutputType(outputType, type);
  char eventType[7];
  getOutputEventType(eventType, type, state);

  StaticJsonDocument<64> json;
  json["gpio"] = GPIO_PINS[index];
  json["type"] = outputType;
  json["event"] = eventType;

  if (!oxrs.publishStatus(json.as<JsonVariant>()))
  {
    oxrs.print(F("[digio] [failover] "));
    serializeJson(json, oxrs);
    oxrs.println();

    // TODO: add failover handling code here
  }
}

/**
  Config handler
 */
void inputConfigSchema(JsonObject json)
{
  JsonObject type = json.createNestedObject("type");
  type["title"] = "Type (defaults to 'switch')";
  createInputTypeEnum(type);

  JsonObject invert = json.createNestedObject("invert");
  invert["title"] = "Invert";
  invert["type"] = "boolean";

  JsonObject disabled = json.createNestedObject("disabled");
  disabled["title"] = "Disabled";
  disabled["type"] = "boolean";
}

void outputConfigSchema(JsonObject json)
{
  JsonObject type = json.createNestedObject("type");
  type["title"] = "Type (defaults to 'relay')";
  createOutputTypeEnum(type);

  JsonObject timerSeconds = json.createNestedObject("timerSeconds");
  timerSeconds["title"] = "Timer (seconds, defaults to 60s)";
  timerSeconds["type"] = "integer";
  timerSeconds["minimum"] = 1;

  JsonObject interlockGpio = json.createNestedObject("interlockGpio");
  interlockGpio["title"] = "Interlock GPIO";
  createGpioPinEnum(interlockGpio);
}

void setConfigSchema()
{
  // Define our config schema
  DynamicJsonDocument json(JSON_CONFIG_MAX_SIZE);

  JsonObject gpios = json.createNestedObject("gpios");
  gpios["title"] = "GPIO Configuration";
  gpios["description"] = "Add configuration for each GPIO in use on your device.";
  gpios["type"] = "array";

  JsonObject items = gpios.createNestedObject("items");
  items["type"] = "object";

  JsonObject properties = items.createNestedObject("properties");

  JsonObject gpio = properties.createNestedObject("gpio");
  gpio["title"] = "GPIO Pin";
  createGpioPinEnum(gpio);

  JsonObject type = properties.createNestedObject("type");
  type["title"] = "GPIO Type";
  createGpioTypeEnum(type);

  JsonObject dependencies = items.createNestedObject("dependencies");
  JsonObject dependenciesType = dependencies.createNestedObject("type");
  JsonArray dependenciesOneOf = dependenciesType.createNestedArray("oneOf");

  JsonObject input = createGpioTypeDependency(dependenciesOneOf, GPIO_INPUT);
  input["title"] = "Input";
  input["type"] = "object";
  inputConfigSchema(input.createNestedObject("properties"));

  JsonObject output = createGpioTypeDependency(dependenciesOneOf, GPIO_OUTPUT);
  output["title"] = "Output";
  output["type"] = "object";
  outputConfigSchema(output.createNestedObject("properties"));

  JsonArray required = items.createNestedArray("required");
  required.add("gpio");
  required.add("type");

  // Pass our config schema down to the OXRS library
  oxrs.setConfigSchema(json.as<JsonVariant>());
}

uint8_t getIndex(JsonVariant json)
{
  if (!json.containsKey("gpio"))
  {
    oxrs.println(F("[digio] missing gpio"));
    return INVALID_GPIO_PIN;
  }
  
  uint8_t gpio = json["gpio"].as<uint8_t>();
  uint8_t index = getIndexFromGpio(gpio);

  if (index == INVALID_GPIO_PIN)
  {
    oxrs.println(F("[digio] invalid gpio, doesn't match a supported pin"));
    return INVALID_GPIO_PIN;
  }

  return index;
}

void jsonInputConfig(uint8_t index, JsonVariant json)
{
  if (json.containsKey("type"))
  {
    uint8_t inputType = parseInputType(json["type"]);    

    if (inputType != INVALID_INPUT_TYPE)
    {
      oxrsInput.setType(index, inputType);
    }
  }
  
  if (json.containsKey("invert"))
  {
    oxrsInput.setInvert(index, json["invert"].as<bool>());
  }

  if (json.containsKey("disabled"))
  {
    oxrsInput.setDisabled(index, json["disabled"].as<bool>());
  }
}

void jsonOutputConfig(uint8_t index, JsonVariant json)
{
  if (json.containsKey("type"))
  {
    uint8_t outputType = parseOutputType(json["type"]);

    if (outputType != INVALID_OUTPUT_TYPE)
    {
      oxrsOutput.setType(index, outputType);
    }
  }

  if (json.containsKey("timerSeconds"))
  {
    if (json["timerSeconds"].isNull())
    {
      oxrsOutput.setTimer(index, DEFAULT_TIMER_SECS);
    }
    else
    {
      oxrsOutput.setTimer(index, json["timerSeconds"].as<uint8_t>());
    }
  }

  if (json.containsKey("interlockGpio"))
  {
    // If an empty message then treat as 'unlocked' - i.e. interlock with ourselves
    if (json["interlockGpio"].isNull())
    {
      oxrsOutput.setInterlock(index, index);
    }
    else
    {
      uint8_t interlockGpio = json["interlockGpio"].as<uint8_t>();
      uint8_t interlockIndex = getIndexFromGpio(interlockGpio);

      if (interlockIndex == INVALID_GPIO_PIN)
      {
        oxrs.println(F("[digio] invalid interlock GPIO"));
      }
      else
      {
        oxrsOutput.setInterlock(index, interlockIndex);
      }
    }
  }
}

void jsonGpioConfig(JsonVariant json)
{
  uint8_t index = getIndex(json);
  if (index == INVALID_GPIO_PIN) return;

  // See if type is explicitly defined, otherwise determine from the config
  uint8_t gpioType = INVALID_GPIO_TYPE;

  if (json.containsKey("type"))
  {
    gpioType = parseGpioType(json["type"]);
  }
  else
  {
    if (json.containsKey("input"))
    {
      gpioType = GPIO_INPUT;
    }
    else if (json.containsKey("output"))
    {
      gpioType = GPIO_OUTPUT;
    }
  }

  // Ignore if an invalid configuration payload
  if (gpioType == INVALID_GPIO_TYPE) 
    return;

  // Store the GPIO type and setup the physical pin
  setGpioType(index, gpioType);

  // Parse and load any type specific config
  if (json.containsKey("input") && gpioType == GPIO_INPUT)
  {
    jsonInputConfig(index, json["input"]);
  }
  if (json.containsKey("output") && gpioType == GPIO_OUTPUT)
  {
    jsonOutputConfig(index, json["output"]);
  }
}

void jsonConfig(JsonVariant json)
{
  if (json.containsKey("gpios"))
  {
    for (JsonVariant gpio : json["gpios"].as<JsonArray>())
    {
      jsonGpioConfig(gpio);    
    }
  }
}

/**
  Command handler
 */
void setCommandSchema()
{
  // Define our config schema
  DynamicJsonDocument json(JSON_COMMAND_MAX_SIZE);

  JsonObject gpios = json.createNestedObject("gpios");
  gpios["title"] = "GPIO Commands";
  gpios["description"] = "Send commands to one or more GPIOs on your device. You can only send commands to GPIOs which have been configured as 'output'. The type is used to validate the configuration for this output matches the command. Supported commands are 'on' or 'off' to change the output state, or 'query' to publish the current state to MQTT.";
  gpios["type"] = "array";

  JsonObject items = gpios.createNestedObject("items");
  items["type"] = "object";

  JsonObject properties = items.createNestedObject("properties");

  JsonObject gpio = properties.createNestedObject("gpio");
  gpio["title"] = "GPIO Pin";
  createGpioPinEnum(gpio);

  JsonObject type = properties.createNestedObject("type");
  type["title"] = "Type";
  createOutputTypeEnum(type);

  JsonObject command = properties.createNestedObject("command");
  command["title"] = "Command";
  command["type"] = "string";
  JsonArray commandEnum = command.createNestedArray("enum");
  commandEnum.add("query");
  commandEnum.add("on");
  commandEnum.add("off");

  JsonArray required = items.createNestedArray("required");
  required.add("gpio");
  required.add("command");

  // Pass our command schema down to the GPIO32 library
  oxrs.setCommandSchema(json.as<JsonVariant>());
}

void jsonGpioCommand(JsonVariant json)
{
  uint8_t index = getIndex(json);
  if (index == INVALID_GPIO_PIN) return;

  // Check this GPIO pin is configured as an output
  if (gpioTypes[index] != GPIO_OUTPUT)
  {
    oxrs.println(F("[digio] command received for GPIO not configured as output"));
    return;
  }

  // Get the output type for this pin
  uint8_t type = oxrsOutput.getType(index);

  if (json.containsKey("type"))
  {
    if (parseOutputType(json["type"]) != type)
    {
      oxrs.println(F("[digio] command type doesn't match configured type"));
      return;
    }
  }

  if (json.containsKey("command"))
  {
    if (json["command"].isNull() || strcmp(json["command"], "query") == 0)
    {
      // Publish a status event with the current state
      uint8_t state = digitalRead(GPIO_PINS[index]);
      publishOutputEvent(index, type, state);
    }
    else
    {
      // Send this command down to our output handler to process
      if (strcmp(json["command"], "on") == 0)
      {
        oxrsOutput.handleCommand(0, index, RELAY_ON);
      }
      else if (strcmp(json["command"], "off") == 0)
      {
        oxrsOutput.handleCommand(0, index, RELAY_OFF);
      }
      else
      {
        oxrs.println(F("[digio] invalid command"));
      }
    }
  }
}

void jsonCommand(JsonVariant json)
{
  if (json.containsKey("gpios"))
  {
    for (JsonVariant gpio : json["gpios"].as<JsonArray>())
    {
      jsonGpioCommand(gpio);
    }
  }
}

/**
  Event handlers
*/
void inputEvent(uint8_t id, uint8_t input, uint8_t type, uint8_t state)
{
  // Publish the event
  publishInputEvent(input, type, state);
}

void outputEvent(uint8_t id, uint8_t output, uint8_t type, uint8_t state)
{
  // Update the GPIO pin - i.e. turn the relay on/off (LOW/HIGH)
  digitalWrite(GPIO_PINS[output], state);

  // Publish the event
  publishOutputEvent(output, type, state);
}

/**
  Setup
*/
void setup()
{
  // Start serial and let settle
  Serial.begin(SERIAL_BAUD_RATE);
  delay(1000);
  Serial.println(F("[digio] starting up..."));
  Serial.println(F("[digio] using GPIOs for digital I/O..."));

  // Initialse our GPIO config array (defaulting to inputs)
  for (uint8_t index = 0; index < GPIO_COUNT; index++)
  {
    setGpioType(index, GPIO_INPUT);
  }

  // Initialise input handlers (default to SWITCH)
  oxrsInput.begin(inputEvent, SWITCH);

  // Initialise output handlers (default to RELAY)
  oxrsOutput.begin(outputEvent, RELAY);

  // Start hardware
  oxrs.begin(jsonConfig, jsonCommand);

  // Set up config schema (for self-discovery and adoption)
  setConfigSchema();
  setCommandSchema();
}

/**
  Main processing loop
*/
void loop()
{
  // Let hardware handle any events etc
  oxrs.loop();

  // Check for any input events
  oxrsInput.process(0, readInputs());

  // Check for any output events
  oxrsOutput.process();

  // required to give background processes a chance
  delay(1);
}
