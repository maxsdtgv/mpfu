#include "bootloader.h"

bool KeyBLRequired(void){
	if (IO_RB0_GetValue() == 1) { 
		return true;
	} 
		return false;
}

void ClearArray(uint8_t *array, uint8_t len){
	uint8_t i = 0;
    for (i = 0; i != len; i++){array[i]=0x00;} // Clear array to send
    return;
}

bool DefineError(uint8_t *send_frame){
	send_frame[0] = 0x02;     		// Number of bytes to send in response
	send_frame[1] = ERROR_CODE;     // Error code
	return true;
}

//bool PingRequest(uint8_t *recv_frame, uint8_t *send_frame){
//	uint16_t dbyte = 0;
//
//	send_frame[0] = 0x04;	// Length of array
//	send_frame[1] = PING_REQUEST;
//	send_frame[2] = MAJOR_VERSION;
//	send_frame[3] = MINOR_VERSION;
//	return true;
//}

//bool EraseRowMem(uint8_t *recv_frame, uint8_t *send_frame){
//	//uint16_t def_addr = 0;
//	//def_addr = (uint16_t)(((recv_frame[2] << 8) & 0xFF00) | (recv_frame[3] & 0x00FF));
//	//FLASH_Erase(def_addr);
//
//	FLASH_Erase(recv_frame);
//
//	send_frame[0] = 0x02;	// Length of array
//	send_frame[1] = ERASE_ROW_MEM;	
//	return true;
//}

bool ReadFromMem(uint8_t *recv_frame, uint8_t *send_frame){
	uint8_t i = 0;
	uint16_t dbyte = 0;
	uint16_t def_addr = 0;
	def_addr = (uint16_t)(((recv_frame[2] << 8) & 0xFF00) | (recv_frame[3] & 0x00FF));
	if (recv_frame[2] == 0x80){
		dbyte = FLASH_Read(def_addr);
		send_frame[2] = (uint8_t)((dbyte & 0xFF00) >> 8);
		send_frame[3] = (uint8_t)(dbyte & 0x00FF);
		i++; i++;
	} else {
		for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
			dbyte = FLASH_Read(def_addr);	
			send_frame[i+2] = (uint8_t)((dbyte & 0xFF00) >> 8);
			send_frame[i+3] = (uint8_t)(dbyte & 0x00FF);
			def_addr++;
		}
	}
	send_frame[0] = i + 2;	// Length of array
	send_frame[1] = READ_FROM_MEM;	
	return true;
}

bool WriteToMem(uint8_t *recv_frame, uint8_t *send_frame){
	bool result;
	uint16_t flash_addr;

	// Config-space writes (addrH==0x80) go straight through; they are not app
	// blocks and must not pass through the reset/vector relocation logic.
	if (recv_frame[2] == 0x80){
		result = FLASH_Write(recv_frame);
	} else {
		flash_addr = (uint16_t)(((recv_frame[2] << 8) & 0xFF00) | (recv_frame[3] & 0x00FF));
		result = WriteAppBlock(flash_addr, &recv_frame[4]);
	}

	if (result) {
		send_frame[0] = 0x02;	// Length of array
		send_frame[1] = SUCCESS_CODE;
	}
	return result;
}

// Arm the autonomous EEPROM->flash upgrade (command SET_EXT_UPGRADE).
// recv_frame[2..3] = EEPROM byte address of the image. We set IsExtUpgrade and
// the start address in the flags row (WriteBootloaderFlags is a read-modify-write
// that preserves the other flags), acknowledge, flush the UART, then reset the
// MCU. On restart the bootloader sees IsExtUpgrade and runs ExtUpgrade().
bool SetExtUpgrade(uint8_t *recv_frame, uint8_t *send_frame){
	uint8_t buf[MAX_BLOCK_BYTES_SIZE + 4];
	uint8_t i = 0;
	uint16_t dbyte = 0;
	uint16_t def_addr = FLAGS_VECTOR;

	// Read-modify-write the flags row directly here so we can also set the
	// ExtAddr bytes (WriteBootloaderFlags only writes the boolean flags).
	for (i = 0; i != MAX_BLOCK_BYTES_SIZE; i += 2){
		dbyte = FLASH_Read(def_addr);
		buf[FRAME_DATA_OFFSET + i]     = (uint8_t)((dbyte & 0xFF00) >> 8);
		buf[FRAME_DATA_OFFSET + i + 1] = (uint8_t)(dbyte & 0x00FF);
		def_addr++;
	}
	buf[2] = (uint8_t)((FLAGS_VECTOR & 0xFF00) >> 8);
	buf[3] = (uint8_t)(FLAGS_VECTOR & 0x00FF);

	buf[WORD_LO_BYTE(FLAG_OFF_IS_EXT_UPGRADE)] = FLAG_SET_BYTE;
	buf[WORD_LO_BYTE(FLAG_OFF_EXT_ADDR_H)]     = recv_frame[2];
	buf[WORD_LO_BYTE(FLAG_OFF_EXT_ADDR_L)]     = recv_frame[3];

	if (!FLASH_Write(buf)){
		return false;   // caller sends an error frame
	}

	send_frame[0] = 0x02;
	send_frame[1] = SUCCESS_CODE;
	UART_dataWrite(send_frame, send_frame[0]);   // ACK now (we won't return)
	while (!EUSART_is_tx_done()){ }               // let the ACK finish on the wire
	__delay_ms(5);
	asm("reset");                                 // restart -> bootloader runs ExtUpgrade
	return true;                                  // not reached
}

// Write one 32-word application block, applying the bootloader's memory rules.
// This is the single place shared by both flashing paths (UART WriteToMem and
// autonomous ExtUpgrade). All vector RELOCATION is now done by the host: the
// app-vector row (0x3FE0) arrives as an ordinary block. The bootloader only
// enforces its own protection here:
//
//   flash_addr : destination WORD address (must be 32-word aligned)
//   data64     : 64 bytes = 32 words, big-endian (H,L per word)
//
// Rules:
//   * reset row (0x0000): keep the existing bootloader trampoline in words 0-3,
//     take the app data for words 0x0004-0x001F from data64.
//   * bootloader code (0x3800-0x3FBF): refuse (self-protection).
//   * flags row (0x3FC0): refuse (owned by the bootloader).
//   * anything else (incl. the app-vector row 0x3FE0): write data64 as-is.
// Returns true on success, false if the block was rejected/failed.
bool WriteAppBlock(uint16_t flash_addr, uint8_t *data64){
	uint8_t frame[MAX_BLOCK_BYTES_SIZE + 4];
	uint8_t i = 0;
	uint16_t w = 0;

	// Protect the bootloader's own code region and its flags row.
	if (flash_addr >= BL_CODE_START && flash_addr <= BL_CODE_END){
		return false;
	}
	if (flash_addr == FLAGS_VECTOR){
		return false;
	}

	frame[2] = (uint8_t)((flash_addr & 0xFF00) >> 8);
	frame[3] = (uint8_t)(flash_addr & 0x00FF);

	if (flash_addr == RESET_ROW_ADDR){
		// Words 0-3: keep the bootloader's own reset trampoline that is already
		// in flash (it may be a two-step MOVLP;GOTO through 0x0002).
		for (i = 0; i < RESET_VECTOR_NWORDS; i++){
			w = FLASH_Read(RESET_ROW_ADDR + i);
			frame[4 + i*2]     = (uint8_t)((w & 0xFF00) >> 8);
			frame[4 + i*2 + 1] = (uint8_t)(w & 0x00FF);
		}
		// Words 4..31: application data from the incoming block.
		for (i = RESET_VECTOR_NWORDS*2; i < MAX_BLOCK_BYTES_SIZE; i++){
			frame[4 + i] = data64[i];
		}
		return FLASH_Write(frame);
	}

	// Ordinary block (application data, or the host-built app-vector row): as-is.
	for (i = 0; i < MAX_BLOCK_BYTES_SIZE; i++){
		frame[4 + i] = data64[i];
	}
	return FLASH_Write(frame);
}

void StartApp(void){
    IO_RE0_SetLow();
    IO_RE1_SetLow();
    asm ("pagesel " str(APP_RESET_VECTOR));
    asm ("goto " str(APP_RESET_VECTOR));

}

// Fletcher-16 over a byte buffer (table-free; matches host imagev2.h).
// Uses conditional subtraction instead of "% 255" — mathematically identical
// (s1,s2 stay < 2*255 per step) but avoids the expensive software division on
// the PIC.
//
// NOTE: guarded by USE_FLETCHER (see bootloader.h). It is DISABLED by default
// on the PIC16F1789 to save flash (the bootloader already uses ~13% and the
// Fletcher path costs ~130 words + needs the BL region extended below 0x3800).
// Integrity of a Wi-Fi-delivered image can instead be checked by reading the
// EEPROM back over the same link and verifying on the host. Enable USE_FLETCHER
// on parts with more flash (and extend the BL ROM region accordingly).
#ifdef USE_FLETCHER
static uint16_t fletcher16_update(uint16_t state, uint8_t *data, uint8_t len){
	uint16_t s1 = (uint16_t)(state & 0xFF);
	uint16_t s2 = (uint16_t)(state >> 8);
	uint8_t i;
	for (i = 0; i < len; i++){
		s1 += data[i];
		if (s1 >= 255) s1 -= 255;
		s2 += s1;
		if (s2 >= 255) s2 -= 255;
	}
	return (uint16_t)((s2 << 8) | s1);
}
#endif

// Read 64 bytes from the external EEPROM at byte address ee_addr into dst[64].
static void ExtReadBlock(uint16_t ee_addr, uint8_t *dst){
	uint8_t req[4];
	uint8_t resp[MAX_BLOCK_BYTES_SIZE + 2];
	uint8_t i;
	req[2] = (uint8_t)((ee_addr & 0xFF00) >> 8);
	req[3] = (uint8_t)(ee_addr & 0x00FF);
	ReadFromSerialEEPROM(req, resp);     // resp[2..65] = 64 data bytes
	for (i = 0; i < MAX_BLOCK_BYTES_SIZE; i++){
		dst[i] = resp[i + 2];
	}
}

// Autonomous firmware upgrade from the external EEPROM (unified image v2).
// The EEPROM holds: [64-byte header][block_count x {addr(2), data(64)}].
// The header carries magic 'MPFU', format_version, device_id, block_count and a
// Fletcher-16 over the blocks area. Each block is programmed to its own address
// through the shared WriteAppBlock() (which enforces protection). The result is
// recorded in BLFlags.StatusCodeExtUpgrade. Optional identity/integrity checks
// are compiled in only when their feature flags are enabled.
void ExtUpgrade(void){
	uint16_t ee_base = BLFlags.StartAddrExtUpgrade;
	uint8_t  header[MAX_BLOCK_BYTES_SIZE];
	uint8_t  tmp[MAX_BLOCK_BYTES_SIZE];
	uint8_t  block[MAX_BLOCK_BYTES_SIZE];
	uint16_t block_count, blk;
	uint16_t block_off, flash_addr;
#ifdef USE_FLETCHER
	uint16_t crc_stored, crc_calc, total, off;
#endif

	// 1. Header
	ExtReadBlock(ee_base, header);
	if (header[IMGV2_OFF_MAGIC+0] != 'M' || header[IMGV2_OFF_MAGIC+1] != 'P' ||
	    header[IMGV2_OFF_MAGIC+2] != 'F' || header[IMGV2_OFF_MAGIC+3] != 'U'){
		BLFlags.StatusCodeExtUpgrade = EXTUP_STATUS_BAD_MAGIC;
		return;
	}

#ifdef USE_VERSION_CHECK
	if (header[IMGV2_OFF_VERSION] != IMGV2_FORMAT_VERSION){
		BLFlags.StatusCodeExtUpgrade = EXTUP_STATUS_BAD_VERSION;
		return;
	}
#endif

#ifdef USE_DEVICE_ID_CHECK
	{
		uint16_t img_dev = (uint16_t)((header[IMGV2_OFF_DEVICE_ID] << 8) | header[IMGV2_OFF_DEVICE_ID+1]);
		if (img_dev != IMGV2_DEVICE_ANY && img_dev != FLASH_Read(DEVID_ADDR)){
			BLFlags.StatusCodeExtUpgrade = EXTUP_STATUS_BAD_DEVICE;
			return;
		}
	}
#endif

	block_count = (uint16_t)((header[IMGV2_OFF_BLOCK_COUNT] << 8) | header[IMGV2_OFF_BLOCK_COUNT+1]);

#ifdef USE_FLETCHER
	// Verify Fletcher-16 over the whole blocks area BEFORE touching flash.
	crc_stored = (uint16_t)((header[IMGV2_OFF_CRC] << 8) | header[IMGV2_OFF_CRC+1]);
	total = (uint16_t)(block_count * IMGV2_BLOCK_SIZE);
	crc_calc = 0;
	for (off = 0; off < total; off += MAX_BLOCK_BYTES_SIZE){
		uint8_t chunk = (uint8_t)((total - off) < MAX_BLOCK_BYTES_SIZE ? (total - off) : MAX_BLOCK_BYTES_SIZE);
		ExtReadBlock(ee_base + IMGV2_HEADER_SIZE + off, tmp);
		crc_calc = fletcher16_update(crc_calc, tmp, chunk);
	}
	if (crc_calc != crc_stored){
		BLFlags.StatusCodeExtUpgrade = EXTUP_STATUS_BAD_CRC;
		return;   // flash untouched
	}
#endif

	// 2. Program each block to its own address via the shared WriteAppBlock().
	//    Block entry i lives at header + i*66: [addr(2)][data(64)]. We read the
	//    2 address bytes and the 64 data bytes (they straddle a 64-byte boundary,
	//    so two EEPROM reads per block).
	for (blk = 0; blk < block_count; blk++){
		block_off = (uint16_t)(IMGV2_HEADER_SIZE + blk * IMGV2_BLOCK_SIZE);
		ExtReadBlock(ee_base + block_off, tmp);              // tmp[0..1] = addr
		flash_addr = (uint16_t)((tmp[0] << 8) | tmp[1]);
		ExtReadBlock(ee_base + block_off + IMGV2_BLOCK_ADDR_BYTES, block); // 64 data
		WriteAppBlock(flash_addr, block);
	}

	BLFlags.StatusCodeExtUpgrade = EXTUP_STATUS_OK;
	return;
}
