uint32_t ui32EEPROMInit;
uint32_t pui32Data[2];
uint32_t pui32Read[2];
//
// Enable the EEPROM module.
//
SysCtlPeripheralEnable(SYSCTL_PERIPH_EEPROM0);
//
// Wait for the EEPROM module to be ready.
//
while(!SysCtlPeripheralReady(SYSCTL_PERIPH_EEPROM0))
{
}
//
// Wait for the EEPROM Initialization to complete
//
ui32EEPROMInit = EEPROMInit();
//
// Check if the EEPROM Initialization returned an error
// and inform the application
//
if(ui32EEPROMInit != EEPROM_INIT_OK)
{
while(1)
{
}
}
//
// Program some data into the EEPROM at address 0x400.
//
pui32Data[0] = 0x12345678;
pui32Data[1] = 0x56789abc;
EEPROMProgram(pui32Data, 0x400, sizeof(pui32Data));
//
// Read it back.
//
EEPROMRead(pui32Read, 0x400, sizeof(pui32Read));
