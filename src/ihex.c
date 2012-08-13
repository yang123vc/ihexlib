/**
 *****************************************************************************
 * @file ihex.c
 * @brief C-Bibliothek um Intel Hex-Files zu manipulieren
 * @author Roman Buchert (roman.buchert@googlemail.com)
 *****************************************************************************/
/*****************************************************************************/

/*
 *****************************************************************************
 * INCLUDE-Dateien
 *****************************************************************************/
#include <ihex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief Wandelt einen Puffer mit Binärdaten in HEX-Records
 * @param *inBuf Zeiger auf Puffer mit Binärdaten
 * @param inBufSize Größe des Puffers mit Binärdaten
 * @param DataLen max. Länge der Daten.
 * @param **outBuf Zeiger auf einen Puffer der die Hex-Records beinhaltet
 * @param *outBufSize Größe des Puffers mit den Hex-Records
 * @return 0: Alles o.k. \n
 * 		   -ENOMEM		: konnte kein Speicher für outBuf allokieren.
 *****************************************************************************/
__s16 ihexBin2Ihex(const __s8 *inBuf, __u32 inBufSize, __u8 DataLen,
					__s8 **outBuf, __u32 *outBufSize)
{
	__u32 Cntr;
	THexRecord Record;
	__s8 *RecordString = NULL;
	__u16 *AdrPtr = NULL;
	__s8 *tmpBuf = NULL;
	__s16 RetVal = 0;

	*outBufSize = 0;

	//Erstelle HEX-Records
	for (Cntr = 0; Cntr < inBufSize; Cntr++)
	{
		//Erstelle den Segemt Adress Record
		if ((Cntr & 0xFFFF) == 0)
		{
			Record.RecordMark = ':';
			Record.RecLen = 0x02;
			Record.LoadOffset = 0x0000;
			Record.RecTyp = rtXLA;
			AdrPtr = (__u16*) &Record.Data[0];
			*AdrPtr = (__u16) (Cntr / 0x10000);
			ihexCalcChksum(&Record);
			RecordString = NULL;
			if (ihexRecord2String(Record, &RecordString) != 0)
			{
				RetVal = (-ENOMEM);
				goto exitBin2Ihex;
			}
			if (tmpBuf == NULL)
			{

				if ((tmpBuf = malloc(1 + strlen((char*) RecordString))) == NULL)
				{
					RetVal = (-ENOMEM);
					goto exitBin2Ihex;
				}
				strcpy((char*) tmpBuf, (char*) RecordString);
			}
			else
			{

				if ((tmpBuf = realloc(tmpBuf,1 + strlen((char*) tmpBuf) + strlen((char*) RecordString))) == NULL)
				{
					RetVal = (-ENOMEM);
					goto exitBin2Ihex;
				}
				strcat((char*) tmpBuf, (char*) RecordString);
			}
			free(RecordString);
			RecordString = NULL;
		}

		//Fülle Header
		if ((Cntr % DataLen) == 0)
		{
			Record.RecordMark = ':';
			if ((Cntr + DataLen) > inBufSize)
				Record.RecLen = inBufSize - Cntr;
			else
				Record.RecLen = DataLen;

			Record.LoadOffset = Cntr;
			Record.RecTyp=rtData;
		}
		//Fülle Daten
		Record.Data[Cntr%DataLen] = inBuf[Cntr];

		//Wenn Datensatz voll, speichern
		if (((Cntr % DataLen) == (DataLen - 1)) || ((Cntr + 1) == inBufSize))
		{
			ihexCalcChksum(&Record);
			RecordString = NULL;
			if (ihexRecord2String(Record, &RecordString) != 0)
			{
				RetVal = (-ENOMEM);
				goto exitBin2Ihex;
			}

			if ((tmpBuf = realloc(tmpBuf, 1 + strlen((char*) tmpBuf) + strlen((char*) RecordString))) == NULL)
			{
				RetVal = (-ENOMEM);
				goto exitBin2Ihex;
			}

			strcat((char*) tmpBuf, (char*) RecordString);

			free(RecordString);
			RecordString = NULL;
		}
	}

	//Enderecord schreiben
	Record.RecordMark = ':';
	Record.RecLen = 0x00;
	Record.LoadOffset = 0x0000;
	Record.RecTyp = rtEOF;
	Record.ChkSum = 0xFF;
	RecordString = NULL;
	if(ihexRecord2String(Record, &RecordString) != 0)
	{
		RetVal = (-ENOMEM);
		goto exitBin2Ihex;
	}


	if ((tmpBuf = realloc(tmpBuf, 1 + strlen((char*) tmpBuf) + strlen((char*) RecordString))) == NULL)
	{
		free (tmpBuf);
		free (RecordString);
		return (-ENOMEM);
	}
	strcat((char*) tmpBuf, (char*) RecordString);

	*outBuf = tmpBuf;
	*outBufSize = strlen((char*) tmpBuf);

exitBin2Ihex:
	if (RetVal != 0)
		free (tmpBuf);

	free (RecordString);

	return (RetVal);
}
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief Erstellt aus einer Hex-Record-Struktur einen Hex-String
 * @param record Datensatz mit dem Hex-Record
 * @param **string Zeiger auf String mit dem Hex-Record
 * @return 	0: Alles o.k.\n
 * 		   -ENOMEM		: konnte kein Speicher für Strings allokieren.
 *****************************************************************************/
__s16 ihexRecord2String(THexRecord record, __s8 **String)
{
	__u8 Cntr;
	__u8 TmpString[10];
	//Speicher für String allokieren
	*String = malloc(15 + (record.RecLen<<1));
	if (*String == 0)
		return (-ENOMEM);
	sprintf((char*)*String, ":%2.2X%4.4X%2.2X",
			 (__u8)record.RecLen, (__u16)record.LoadOffset, (__u8)record.RecTyp);
	//Daten hinzufügen
	for(Cntr = 0; Cntr < record.RecLen; Cntr++)
	{
		sprintf((char*)(TmpString),"%2.2X",
		(__u8)record.Data[Cntr]);
		strcat((char*)*String,(char*)TmpString);
	}

	//CRC und Endekennung hinzufügen
	sprintf((char*)(TmpString),"%2.2X\r\n",(__u8)record.ChkSum);
	strcat((char*)*String,(char*)TmpString);
	return (0);
}
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief Erstellt aus einem Hex-Record-String eine Hex-Record-Struktur
 * @param *string String mit dem Hex-Record
 * @param *record Zeiger auf Datensatz mit dem Hex-Record
 * @return 	0: Alles o.k.\n
 *****************************************************************************/
__s16 ihexString2Record(__s8* string, THexRecord *record)
{
	__u8 tmpString[5];
	__u8 Cntr;

	//Recordmark setzen
	record->RecordMark=(__u8) string[0];
	//Datenlänge setzen
	memset(tmpString, 0 ,sizeof(tmpString));
	strncpy((char*)tmpString, (char*) &string[1], 2);
	record->RecLen = (__u8) strtoul((char*)tmpString, NULL, 16);
	//Load Offset setzen
	memset(tmpString, 0 ,sizeof(tmpString));
	strncpy((char*)tmpString, (char*)&string[3],4);
	record->LoadOffset = (__u16) strtoul((char*)tmpString, NULL, 16);
	//Recordtype setzen
	memset(tmpString, 0 ,sizeof(tmpString));
	strncpy((char*)tmpString, (char*)&string[7],2);
	record->RecTyp = (__u8) strtoul((char*)tmpString, NULL, 16);
	//Daten füllen
	for (Cntr = 0; Cntr < record->RecLen; Cntr++)
	{
		memset(tmpString, 0 ,sizeof(tmpString));
		strncpy((char*)tmpString, (char*)&string[9+(Cntr<<1)], 2);
		record->Data[Cntr] = (__u8) strtoul((char*)tmpString, NULL, 16);
	}
	//Checksumme setzen
	memset(tmpString, 0 ,sizeof(tmpString));
	strncpy((char*)tmpString, (char*)&string[9+(Cntr<<1)],2);
	record->ChkSum = (__u8) strtoul((char*)tmpString, NULL, 16);

	return (0);
}
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief Berechnet die Checksumme des HEX-Record
 * @param *record Zeiger auf Struktur des Hexrecord
 * @return 0: Alles o.k.
 *****************************************************************************/
__s16 ihexCalcChksum(THexRecord *record)
{
	__u8 CheckSum = 0;
	__u8 *BytePtr;
	__u16 Cntr;

	BytePtr = (__u8*) record;

	BytePtr++;
	for (Cntr = 0; Cntr < (record->RecLen + 4); Cntr++ )
	{
		CheckSum += *BytePtr;
		BytePtr++;
	}

	record->ChkSum = -CheckSum;
	return (0);
}
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief Prüft die Checksumme des HEX-Record
 * @param record Hexrecord
 * @return 0	: Prüfsumme stimmt. \n
 * 		   !=0	: Prüfsumme ist falsch.
 *****************************************************************************/
__s16 ihexCheckChksum(THexRecord record)
{
	__u8 TmpCheckSum;
	TmpCheckSum = record.ChkSum;

	ihexCalcChksum(&record);
	return (record.ChkSum - TmpCheckSum);
}
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief
 * @param
 * @return
 *****************************************************************************/
/*****************************************************************************/

/**
 *****************************************************************************
 * @brief
 * @param
 * @return
 *****************************************************************************/
/*****************************************************************************/

