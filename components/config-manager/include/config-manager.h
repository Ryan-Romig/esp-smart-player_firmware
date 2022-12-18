void NVS_Write_String(const char* name, const char* key, const char* stringVal);
char* NVS_Read_String(const char* name, const char* key);
void init_nvs();
#define NVS_NAMESPACE "WIFI"

