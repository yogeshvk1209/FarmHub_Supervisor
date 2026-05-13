#include <pgmspace.h>

const char WIFI_SSID[] = "some_ssid";
const char WIFI_PASSWORD[] = "some_pass";

const char AWS_IOT_ENDPOINT[] = "some.iot.ap-south-1.amazonaws.com";

// Paste the content of your -certificate.pem.crt here
static const char AWS_CERT_CRT[] PROGMEM = R"KEY(
)KEY";

// Paste the content of your -private.pem.key here
static const char AWS_CERT_PRIVATE[] PROGMEM = R"KEY(
)KEY";

// Paste the AmazonRootCA1.pem here
static const char AWS_CERT_CA[] PROGMEM = R"KEY(
)KEY";

#define AWS_LAMBDA_URL "https://xxxx.execute-api.ap-south-1.amazonaws.com/prod/sign"
