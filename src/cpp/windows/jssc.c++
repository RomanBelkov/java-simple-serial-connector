/* jSSC (Java Simple Serial Connector) - serial port communication library.
* © Alexey Sokolov (scream3r), 2010-2014.
*   Roman Belkov <roman.belkov@gmail.com>, 2017
*
* This file is part of jSSC.
*
* jSSC is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* jSSC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with jSSC.  If not, see <http://www.gnu.org/licenses/>.
*
* If you use jSSC in public project you can inform me about this by e-mail,
* of course if you want it.
*
* e-mail: scream3r.org@gmail.com
* web-site: http://scream3r.org | https://github.com/scream3r/java-simple-serial-connector
*/

#include <algorithm>
#include <exception>
#include <sstream>
#include <iostream>

#include "../jssc_SerialNativeInterface.h"
#include "jssc_win.h"

#include <devpkey.h>

/*
* Get native library version
*/
JNIEXPORT jstring JNICALL Java_jssc_SerialNativeInterface_getNativeLibraryVersion(JNIEnv *env, jobject object) {
	return env->NewStringUTF(jSSC_NATIVE_LIB_VERSION);
}

/*
* Port opening.
*
* In 2.2.0 added useTIOCEXCL (not used in Windows, only for compatibility with _nix version)
* Usage of wstring added by Roman Belkov in post-2.8.0
*/
JNIEXPORT jlong JNICALL Java_jssc_SerialNativeInterface_openPort(JNIEnv *env, jobject object, jstring portName, jboolean useTIOCEXCL) {
	const std::wstring prefix = L"\\\\.\\";
	//const char* port = env->GetStringUTFChars(portName, JNI_FALSE);
	const std::wstring port = jstr2wstr(env, portName);

	std::wstring portFullName = std::wstring(prefix);
	portFullName += port;

	HANDLE hComm = CreateFile(portFullName.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		0,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		0);

	//since 2.3.0 ->
	if (hComm != INVALID_HANDLE_VALUE) {
		DCB *dcb = new DCB();
		if (!GetCommState(hComm, dcb)) {
			CloseHandle(hComm);//since 2.7.0
			hComm = (HANDLE)jssc_SerialNativeInterface_ERR_INCORRECT_SERIAL_PORT;//(-4)Incorrect serial port
		}
		delete dcb;
	}
	else {
		DWORD errorValue = GetLastError();
		if (errorValue == ERROR_ACCESS_DENIED) {
			hComm = (HANDLE)jssc_SerialNativeInterface_ERR_PORT_BUSY;//(-1)Port busy
		}
		else if (errorValue == ERROR_FILE_NOT_FOUND) {
			hComm = (HANDLE)jssc_SerialNativeInterface_ERR_PORT_NOT_FOUND;//(-2)Port not found
		}
	}
	//<- since 2.3.0
	return (jlong)hComm;//since 2.4.0 changed to jlong
};

/*
* Setting serial port params.
*
* In 2.6.0 added flags (not used in Windows, only for compatibility with _nix version)
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_setParams
(JNIEnv *env, jobject object, jlong portHandle, jint baudRate, jint byteSize, jint stopBits, jint parity, jboolean setRTS, jboolean setDTR, jint flags) {
	HANDLE hComm = (HANDLE)portHandle;
	DCB *dcb = new DCB();
	jboolean returnValue = JNI_FALSE;
	if (GetCommState(hComm, dcb)) {
		dcb->BaudRate = baudRate;
		dcb->ByteSize = byteSize;
		dcb->StopBits = stopBits;
		dcb->Parity = parity;

		//since 0.8 ->
		if (setRTS == JNI_TRUE) {
			dcb->fRtsControl = RTS_CONTROL_ENABLE;
		}
		else {
			dcb->fRtsControl = RTS_CONTROL_DISABLE;
		}
		if (setDTR == JNI_TRUE) {
			dcb->fDtrControl = DTR_CONTROL_ENABLE;
		}
		else {
			dcb->fDtrControl = DTR_CONTROL_DISABLE;
		}
		dcb->fOutxCtsFlow = FALSE;
		dcb->fOutxDsrFlow = FALSE;
		dcb->fDsrSensitivity = FALSE;
		dcb->fTXContinueOnXoff = TRUE;
		dcb->fOutX = FALSE;
		dcb->fInX = FALSE;
		dcb->fErrorChar = FALSE;
		dcb->fNull = FALSE;
		dcb->fAbortOnError = FALSE;
		dcb->XonLim = 2048;
		dcb->XoffLim = 512;
		dcb->XonChar = (char)17; //DC1
		dcb->XoffChar = (char)19; //DC3
								  //<- since 0.8

		if (SetCommState(hComm, dcb)) {

			//since 2.1.0 -> previously setted timeouts by another application should be cleared
			COMMTIMEOUTS *lpCommTimeouts = new COMMTIMEOUTS();
			lpCommTimeouts->ReadIntervalTimeout = 0;
			lpCommTimeouts->ReadTotalTimeoutConstant = 0;
			lpCommTimeouts->ReadTotalTimeoutMultiplier = 0;
			lpCommTimeouts->WriteTotalTimeoutConstant = 0;
			lpCommTimeouts->WriteTotalTimeoutMultiplier = 0;
			if (SetCommTimeouts(hComm, lpCommTimeouts)) {
				returnValue = JNI_TRUE;
			}
			delete lpCommTimeouts;
			//<- since 2.1.0
		}
	}
	delete dcb;
	return returnValue;
}

/*
* PurgeComm
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_purgePort
(JNIEnv *env, jobject object, jlong portHandle, jint flags) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD dwFlags = (DWORD)flags;
	return (PurgeComm(hComm, dwFlags) ? JNI_TRUE : JNI_FALSE);
}

/*
* Port closing
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_closePort
(JNIEnv *env, jobject object, jlong portHandle) {
	HANDLE hComm = (HANDLE)portHandle;
	return (CloseHandle(hComm) ? JNI_TRUE : JNI_FALSE);
}

/*
* Set events mask
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_setEventsMask
(JNIEnv *env, jobject object, jlong portHandle, jint mask) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD dwEvtMask = (DWORD)mask;
	return (SetCommMask(hComm, dwEvtMask) ? JNI_TRUE : JNI_FALSE);
}

/*
* Get events mask
*/
JNIEXPORT jint JNICALL Java_jssc_SerialNativeInterface_getEventsMask
(JNIEnv *env, jobject object, jlong portHandle) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD lpEvtMask;
	if (GetCommMask(hComm, &lpEvtMask)) {
		return (jint)lpEvtMask;
	}
	else {
		return -1;
	}
}

/*
* Change RTS line state (ON || OFF)
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_setRTS
(JNIEnv *env, jobject object, jlong portHandle, jboolean enabled) {
	HANDLE hComm = (HANDLE)portHandle;
	if (enabled == JNI_TRUE) {
		return (EscapeCommFunction(hComm, SETRTS) ? JNI_TRUE : JNI_FALSE);
	}
	else {
		return (EscapeCommFunction(hComm, CLRRTS) ? JNI_TRUE : JNI_FALSE);
	}
}

/*
* Change DTR line state (ON || OFF)
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_setDTR
(JNIEnv *env, jobject object, jlong portHandle, jboolean enabled) {
	HANDLE hComm = (HANDLE)portHandle;
	if (enabled == JNI_TRUE) {
		return (EscapeCommFunction(hComm, SETDTR) ? JNI_TRUE : JNI_FALSE);
	}
	else {
		return (EscapeCommFunction(hComm, CLRDTR) ? JNI_TRUE : JNI_FALSE);
	}
}

/*
* Write data to port
* portHandle - port handle
* buffer - byte array for sending
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_writeBytes
(JNIEnv *env, jobject object, jlong portHandle, jbyteArray buffer) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD lpNumberOfBytesTransferred;
	DWORD lpNumberOfBytesWritten;
	OVERLAPPED *overlapped = new OVERLAPPED();
	jboolean returnValue = JNI_FALSE;
	jbyte* jBuffer = env->GetByteArrayElements(buffer, JNI_FALSE);
	overlapped->hEvent = CreateEventA(NULL, true, false, NULL);
	if (WriteFile(hComm, jBuffer, (DWORD)env->GetArrayLength(buffer), &lpNumberOfBytesWritten, overlapped)) {
		returnValue = JNI_TRUE;
	}
	else if (GetLastError() == ERROR_IO_PENDING) {
		if (WaitForSingleObject(overlapped->hEvent, INFINITE) == WAIT_OBJECT_0) {
			if (GetOverlappedResult(hComm, overlapped, &lpNumberOfBytesTransferred, false)) {
				returnValue = JNI_TRUE;
			}
		}
	}
	env->ReleaseByteArrayElements(buffer, jBuffer, 0);
	CloseHandle(overlapped->hEvent);
	delete overlapped;
	return returnValue;
}

/*
* Read data from port
* portHandle - port handle
* byteCount - count of bytes for reading
*/
JNIEXPORT jbyteArray JNICALL Java_jssc_SerialNativeInterface_readBytes
(JNIEnv *env, jobject object, jlong portHandle, jint byteCount) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD lpNumberOfBytesTransferred;
	DWORD lpNumberOfBytesRead;
	OVERLAPPED *overlapped = new OVERLAPPED();
	jbyte *lpBuffer = new jbyte [byteCount];
	jbyteArray returnArray = env->NewByteArray(byteCount);
	overlapped->hEvent = CreateEventA(NULL, true, false, NULL);
	if (ReadFile(hComm, lpBuffer, (DWORD)byteCount, &lpNumberOfBytesRead, overlapped)) {
		env->SetByteArrayRegion(returnArray, 0, byteCount, lpBuffer);
	}
	else if (GetLastError() == ERROR_IO_PENDING) {
		if (WaitForSingleObject(overlapped->hEvent, INFINITE) == WAIT_OBJECT_0) {
			if (GetOverlappedResult(hComm, overlapped, &lpNumberOfBytesTransferred, false)) {
				env->SetByteArrayRegion(returnArray, 0, byteCount, lpBuffer);
			}
		}
	}
	CloseHandle(overlapped->hEvent);
	delete overlapped;
	return returnArray;
}

/*
* Get bytes count in serial port buffers (Input and Output)
*/
JNIEXPORT jintArray JNICALL Java_jssc_SerialNativeInterface_getBuffersBytesCount
(JNIEnv *env, jobject object, jlong portHandle) {
	HANDLE hComm = (HANDLE)portHandle;
	jint returnValues[2];
	returnValues[0] = -1;
	returnValues[1] = -1;
	jintArray returnArray = env->NewIntArray(2);
	DWORD lpErrors;
	COMSTAT *comstat = new COMSTAT();
	if (ClearCommError(hComm, &lpErrors, comstat)) {
		returnValues[0] = (jint)comstat->cbInQue;
		returnValues[1] = (jint)comstat->cbOutQue;
	}
	else {
		returnValues[0] = -1;
		returnValues[1] = -1;
	}
	delete comstat;
	env->SetIntArrayRegion(returnArray, 0, 2, returnValues);
	return returnArray;
}

//since 0.8 ->
const jint FLOWCONTROL_NONE = 0;
const jint FLOWCONTROL_RTSCTS_IN = 1;
const jint FLOWCONTROL_RTSCTS_OUT = 2;
const jint FLOWCONTROL_XONXOFF_IN = 4;
const jint FLOWCONTROL_XONXOFF_OUT = 8;
//<- since 0.8

/*
* Setting flow control mode
*
* since 0.8
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_setFlowControlMode
(JNIEnv *env, jobject object, jlong portHandle, jint mask) {
	HANDLE hComm = (HANDLE)portHandle;
	jboolean returnValue = JNI_FALSE;
	DCB *dcb = new DCB();
	if (GetCommState(hComm, dcb)) {
		dcb->fRtsControl = RTS_CONTROL_ENABLE;
		dcb->fOutxCtsFlow = FALSE;
		dcb->fOutX = FALSE;
		dcb->fInX = FALSE;
		if (mask != FLOWCONTROL_NONE) {
			if ((mask & FLOWCONTROL_RTSCTS_IN) == FLOWCONTROL_RTSCTS_IN) {
				dcb->fRtsControl = RTS_CONTROL_HANDSHAKE;
			}
			if ((mask & FLOWCONTROL_RTSCTS_OUT) == FLOWCONTROL_RTSCTS_OUT) {
				dcb->fOutxCtsFlow = TRUE;
			}
			if ((mask & FLOWCONTROL_XONXOFF_IN) == FLOWCONTROL_XONXOFF_IN) {
				dcb->fInX = TRUE;
			}
			if ((mask & FLOWCONTROL_XONXOFF_OUT) == FLOWCONTROL_XONXOFF_OUT) {
				dcb->fOutX = TRUE;
			}
		}
		if (SetCommState(hComm, dcb)) {
			returnValue = JNI_TRUE;
		}
	}
	delete dcb;
	return returnValue;
}

/*
* Getting flow control mode
*
* since 0.8
*/
JNIEXPORT jint JNICALL Java_jssc_SerialNativeInterface_getFlowControlMode
(JNIEnv *env, jobject object, jlong portHandle) {
	HANDLE hComm = (HANDLE)portHandle;
	jint returnValue = 0;
	DCB *dcb = new DCB();
	if (GetCommState(hComm, dcb)) {
		if (dcb->fRtsControl == RTS_CONTROL_HANDSHAKE) {
			returnValue |= FLOWCONTROL_RTSCTS_IN;
		}
		if (dcb->fOutxCtsFlow == TRUE) {
			returnValue |= FLOWCONTROL_RTSCTS_OUT;
		}
		if (dcb->fInX == TRUE) {
			returnValue |= FLOWCONTROL_XONXOFF_IN;
		}
		if (dcb->fOutX == TRUE) {
			returnValue |= FLOWCONTROL_XONXOFF_OUT;
		}
	}
	delete dcb;
	return returnValue;
}

/*
* Send break for setted duration
*
* since 0.8
*/
JNIEXPORT jboolean JNICALL Java_jssc_SerialNativeInterface_sendBreak
(JNIEnv *env, jobject object, jlong portHandle, jint duration) {
	HANDLE hComm = (HANDLE)portHandle;
	jboolean returnValue = JNI_FALSE;
	if (duration > 0) {
		if (SetCommBreak(hComm) > 0) {
			Sleep(duration);
			if (ClearCommBreak(hComm) > 0) {
				returnValue = JNI_TRUE;
			}
		}
	}
	return returnValue;
}

/*
* Wait event
* portHandle - port handle
*/
JNIEXPORT jobjectArray JNICALL Java_jssc_SerialNativeInterface_waitEvents
(JNIEnv *env, jobject object, jlong portHandle) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD lpEvtMask = 0;
	DWORD lpNumberOfBytesTransferred = 0;
	OVERLAPPED *overlapped = new OVERLAPPED();
	jclass intClass = env->FindClass("[I");
	jobjectArray returnArray;
	boolean functionSuccessful = false;
	overlapped->hEvent = CreateEventA(NULL, true, false, NULL);
	if (WaitCommEvent(hComm, &lpEvtMask, overlapped)) {
		functionSuccessful = true;
	}
	else if (GetLastError() == ERROR_IO_PENDING) {
		if (WaitForSingleObject(overlapped->hEvent, INFINITE) == WAIT_OBJECT_0) {
			if (GetOverlappedResult(hComm, overlapped, &lpNumberOfBytesTransferred, false)) {
				functionSuccessful = true;
			}
		}
	}
	if (functionSuccessful) {
		boolean executeGetCommModemStatus = false;
		boolean executeClearCommError = false;
		DWORD events[9];//fixed since 0.8 (old value is 8)
		jint eventsCount = 0;
		if ((EV_BREAK & lpEvtMask) == EV_BREAK) {
			events[eventsCount] = EV_BREAK;
			eventsCount++;
		}
		if ((EV_CTS & lpEvtMask) == EV_CTS) {
			events[eventsCount] = EV_CTS;
			eventsCount++;
			executeGetCommModemStatus = true;
		}
		if ((EV_DSR & lpEvtMask) == EV_DSR) {
			events[eventsCount] = EV_DSR;
			eventsCount++;
			executeGetCommModemStatus = true;
		}
		if ((EV_ERR & lpEvtMask) == EV_ERR) {
			events[eventsCount] = EV_ERR;
			eventsCount++;
			executeClearCommError = true;
		}
		if ((EV_RING & lpEvtMask) == EV_RING) {
			events[eventsCount] = EV_RING;
			eventsCount++;
			executeGetCommModemStatus = true;
		}
		if ((EV_RLSD & lpEvtMask) == EV_RLSD) {
			events[eventsCount] = EV_RLSD;
			eventsCount++;
			executeGetCommModemStatus = true;
		}
		if ((EV_RXCHAR & lpEvtMask) == EV_RXCHAR) {
			events[eventsCount] = EV_RXCHAR;
			eventsCount++;
			executeClearCommError = true;
		}
		if ((EV_RXFLAG & lpEvtMask) == EV_RXFLAG) {
			events[eventsCount] = EV_RXFLAG;
			eventsCount++;
			executeClearCommError = true;
		}
		if ((EV_TXEMPTY & lpEvtMask) == EV_TXEMPTY) {
			events[eventsCount] = EV_TXEMPTY;
			eventsCount++;
			executeClearCommError = true;
		}
		/*
		* Execute GetCommModemStatus function if it's needed (get lines status)
		*/
		jint statusCTS = 0;
		jint statusDSR = 0;
		jint statusRING = 0;
		jint statusRLSD = 0;
		boolean successGetCommModemStatus = false;
		if (executeGetCommModemStatus) {
			DWORD lpModemStat;
			if (GetCommModemStatus(hComm, &lpModemStat)) {
				successGetCommModemStatus = true;
				if ((MS_CTS_ON & lpModemStat) == MS_CTS_ON) {
					statusCTS = 1;
				}
				if ((MS_DSR_ON & lpModemStat) == MS_DSR_ON) {
					statusDSR = 1;
				}
				if ((MS_RING_ON & lpModemStat) == MS_RING_ON) {
					statusRING = 1;
				}
				if ((MS_RLSD_ON & lpModemStat) == MS_RLSD_ON) {
					statusRLSD = 1;
				}
			}
			else {
				jint lastError = (jint)GetLastError();
				statusCTS = lastError;
				statusDSR = lastError;
				statusRING = lastError;
				statusRLSD = lastError;
			}
		}
		/*
		* Execute ClearCommError function if it's needed (get count of bytes in buffers and errors)
		*/
		jint bytesCountIn = 0;
		jint bytesCountOut = 0;
		jint communicationsErrors = 0;
		boolean successClearCommError = false;
		if (executeClearCommError) {
			DWORD lpErrors;
			COMSTAT *comstat = new COMSTAT();
			if (ClearCommError(hComm, &lpErrors, comstat)) {
				successClearCommError = true;
				bytesCountIn = (jint)comstat->cbInQue;
				bytesCountOut = (jint)comstat->cbOutQue;
				communicationsErrors = (jint)lpErrors;
			}
			else {
				jint lastError = (jint)GetLastError();
				bytesCountIn = lastError;
				bytesCountOut = lastError;
				communicationsErrors = lastError;
			}
			delete comstat;
		}
		/*
		* Create int[][] for events values
		*/
		returnArray = env->NewObjectArray(eventsCount, intClass, NULL);
		/*
		* Set events values
		*/
		for (jint i = 0; i < eventsCount; i++) {
			jint returnValues[2];
			switch (events[i]) {
			case EV_BREAK:
				returnValues[0] = (jint)events[i];
				returnValues[1] = 0;
				goto forEnd;
			case EV_CTS:
				returnValues[1] = statusCTS;
				goto modemStatus;
			case EV_DSR:
				returnValues[1] = statusDSR;
				goto modemStatus;
			case EV_ERR:
				returnValues[1] = communicationsErrors;
				goto bytesAndErrors;
			case EV_RING:
				returnValues[1] = statusRING;
				goto modemStatus;
			case EV_RLSD:
				returnValues[1] = statusRLSD;
				goto modemStatus;
			case EV_RXCHAR:
				returnValues[1] = bytesCountIn;
				goto bytesAndErrors;
			case EV_RXFLAG:
				returnValues[1] = bytesCountIn;
				goto bytesAndErrors;
			case EV_TXEMPTY:
				returnValues[1] = bytesCountOut;
				goto bytesAndErrors;
			default:
				returnValues[0] = (jint)events[i];
				returnValues[1] = 0;
				goto forEnd;
			};
		modemStatus: {
			if (successGetCommModemStatus) {
				returnValues[0] = (jint)events[i];
			}
			else {
				returnValues[0] = -1;
			}
			goto forEnd;
			}
				 bytesAndErrors: {
					 if (successClearCommError) {
						 returnValues[0] = (jint)events[i];
					 }
					 else {
						 returnValues[0] = -1;
					 }
					 goto forEnd;
		}
							 forEnd: {
								 jintArray singleResultArray = env->NewIntArray(2);
								 env->SetIntArrayRegion(singleResultArray, 0, 2, returnValues);
								 env->SetObjectArrayElement(returnArray, i, singleResultArray);
				 };
		}
	}
	else {
		returnArray = env->NewObjectArray(1, intClass, NULL);
		jint returnValues[2];
		returnValues[0] = -1;
		returnValues[1] = (jint)GetLastError();
		jintArray singleResultArray = env->NewIntArray(2);
		env->SetIntArrayRegion(singleResultArray, 0, 2, returnValues);
		env->SetObjectArrayElement(returnArray, 0, singleResultArray);
	};
	CloseHandle(overlapped->hEvent);
	delete overlapped;
	return returnArray;
}

/*
* Get serial port names
*/
JNIEXPORT jobjectArray JNICALL Java_jssc_SerialNativeInterface_getSerialPortNames
(JNIEnv *env, jobject object) {
	HKEY phkResult;
	LPCSTR lpSubKey = "HARDWARE\\DEVICEMAP\\SERIALCOMM\\";
	jobjectArray returnArray = NULL;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, lpSubKey, 0, KEY_READ, &phkResult) == ERROR_SUCCESS) {
		boolean hasMoreElements = true;
		DWORD keysCount = 0;
		char valueName[256];
		DWORD valueNameSize;
		DWORD enumResult;
		while (hasMoreElements) {
			valueNameSize = 256;
			enumResult = RegEnumValueA(phkResult, keysCount, valueName, &valueNameSize, NULL, NULL, NULL, NULL);
			if (enumResult == ERROR_SUCCESS) {
				keysCount++;
			}
			else if (enumResult == ERROR_NO_MORE_ITEMS) {
				hasMoreElements = false;
			}
			else {
				hasMoreElements = false;
			}
		}
		if (keysCount > 0) {
			jclass stringClass = env->FindClass("java/lang/String");
			returnArray = env->NewObjectArray((jsize)keysCount, stringClass, NULL);
			char lpValueName[256];
			DWORD lpcchValueName;
			byte lpData[256];
			DWORD lpcbData;
			DWORD result;
			for (DWORD i = 0; i < keysCount; i++) {
				lpcchValueName = 256;
				lpcbData = 256;
				result = RegEnumValueA(phkResult, i, lpValueName, &lpcchValueName, NULL, NULL, lpData, &lpcbData);
				if (result == ERROR_SUCCESS) {
					env->SetObjectArrayElement(returnArray, i, env->NewStringUTF((char*)lpData));
				}
			}
		}
		CloseHandle(phkResult);
	}
	return returnArray;
}

/*
* Get lines status
*
* returnValues[0] - CTS
* returnValues[1] - DSR
* returnValues[2] - RING
* returnValues[3] - RLSD
*
*/
JNIEXPORT jintArray JNICALL Java_jssc_SerialNativeInterface_getLinesStatus
(JNIEnv *env, jobject object, jlong portHandle) {
	HANDLE hComm = (HANDLE)portHandle;
	DWORD lpModemStat;
	jint returnValues[4];
	for (jint i = 0; i < 4; i++) {
		returnValues[i] = 0;
	}
	jintArray returnArray = env->NewIntArray(4);
	if (GetCommModemStatus(hComm, &lpModemStat)) {
		if ((MS_CTS_ON & lpModemStat) == MS_CTS_ON) {
			returnValues[0] = 1;
		}
		if ((MS_DSR_ON & lpModemStat) == MS_DSR_ON) {
			returnValues[1] = 1;
		}
		if ((MS_RING_ON & lpModemStat) == MS_RING_ON) {
			returnValues[2] = 1;
		}
		if ((MS_RLSD_ON & lpModemStat) == MS_RLSD_ON) {
			returnValues[3] = 1;
		}
	}
	env->SetIntArrayRegion(returnArray, 0, 4, returnValues);
	return returnArray;
}

/*
* Gets properties of given port
*
* Returns a string array of port's properties
*
* ret[0] - product ID (in hex)
* ret[1] - vendor ID (in hex)
* ret[2] - manufacturer
* ret[3] - description
* ret[4] - serial number
*
* NOTE: parts of this code are based on Qt's serialport.
* Since JSSC is already LGPL-licensed, this does not seem like a violation of LGPL rules
*
*/

JNIEXPORT jobjectArray JNICALL Java_jssc_SerialNativeInterface_getPortProperties
(JNIEnv *env, jclass cls, jstring portName) {
	std::wstring wantedPortName = jstr2wstr(env, portName);
	jclass stringClass = env->FindClass("Ljava/lang/String;");
	int itemsCount = 6;
	int retPos = 0;
	jobjectArray ret = env->NewObjectArray(itemsCount, stringClass, NULL);

	std::vector<std::wstring> ports;

	static const struct {
		GUID guid; DWORD flags;
	} setupTokens[] = {
		{ GUID_DEVCLASS_PORTS, DIGCF_PRESENT },
		{ GUID_DEVCLASS_MODEM, DIGCF_PRESENT },
		{ GUID_DEVINTERFACE_COMPORT, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE },
		{ GUID_DEVINTERFACE_MODEM, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE }
	};

	enum { SetupTokensCount = sizeof(setupTokens) / sizeof(setupTokens[0]) };

	for (int i = 0; i < SetupTokensCount; ++i) {
		const HDEVINFO deviceInfoSet = ::SetupDiGetClassDevs(&setupTokens[i].guid, nullptr, nullptr, setupTokens[i].flags);
		if (deviceInfoSet == INVALID_HANDLE_VALUE)
			return ret;

		SP_DEVINFO_DATA deviceInfoData;
		::memset(&deviceInfoData, 0, sizeof(deviceInfoData));
		deviceInfoData.cbSize = sizeof(deviceInfoData);

		DWORD index = 0;
		while (::SetupDiEnumDeviceInfo(deviceInfoSet, index++, &deviceInfoData)) {
			const std::wstring devicePort = std::wstring(devicePortName(deviceInfoSet, &deviceInfoData).c_str());
			if (devicePort.empty() || (devicePort.find(L"LPT") != std::string::npos) || devicePort.compare(wantedPortName) != 0)
				continue;

			if (anyOfPorts(ports, devicePort))
				continue;

			const std::wstring instanceIdentifier = deviceInstanceIdentifier(deviceInfoData.DevInst);

			bool ok = false;
			const uint16_t productIdentifier =
				deviceProductIdentifier(instanceIdentifier, ok);
			addWStringToJavaArray(env, ret, retPos, intToString(productIdentifier));
			retPos++;

			ok = false;
			const uint16_t vendorIdentifier =
				deviceVendorIdentifier(instanceIdentifier, ok);
			addWStringToJavaArray(env, ret, retPos, intToString(vendorIdentifier));
			retPos++;

			const std::wstring manufacturer = trimAfterFirstNull(deviceManufacturer(deviceInfoSet, &deviceInfoData));
			addWStringToJavaArray(env, ret, retPos, manufacturer);
			retPos++;

			const std::wstring description = trimAfterFirstNull(deviceDescription(deviceInfoSet, &deviceInfoData));
			addWStringToJavaArray(env, ret, retPos, description);
			retPos++;

			std::wstring busProvidedDesc = std::wstring();
			if (IsWindows7OrGreater()) {
				busProvidedDesc = trimAfterFirstNull(busProvidedDescription(deviceInfoSet, &deviceInfoData));
			}
			addWStringToJavaArray(env, ret, retPos, busProvidedDesc);
			retPos++;

			const std::wstring serialNumber = trimAfterFirstNull(deviceSerialNumber(instanceIdentifier, deviceInfoData.DevInst));
			addWStringToJavaArray(env, ret, retPos, serialNumber);
			retPos++;

			ports.push_back(devicePort);
		}
		::SetupDiDestroyDeviceInfoList(deviceInfoSet);
	}
	return ret;
}

/*
* Functions below are helper methods. Mainly they are for getSerialPorts (as of 10.07.2017)
*/

static std::wstring deviceRegistryProperty(HDEVINFO deviceInfoSet,
	PSP_DEVINFO_DATA deviceInfoData,
	DWORD property) {

	DWORD dataType = 0;
	std::vector<wchar_t> outputBuffer(MAX_PATH + 1, 0);
	DWORD bytesRequired = MAX_PATH;
	for (;;) {
		if (::SetupDiGetDeviceRegistryProperty(deviceInfoSet, deviceInfoData, property, &dataType,
			reinterpret_cast<PBYTE>(&outputBuffer[0]),
			bytesRequired, &bytesRequired)) {
			break;
		}

		if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER
			|| (dataType != REG_SZ && dataType != REG_EXPAND_SZ)) {
			return std::wstring();
		}
		outputBuffer.resize(bytesRequired / sizeof(wchar_t) + 2, 0);
	}
	return std::wstring(outputBuffer.begin(), outputBuffer.end());
}

static std::wstring busProvidedDescription(HDEVINFO deviceInfoSet,
	PSP_DEVINFO_DATA deviceInfoData)
{
	DWORD dataType = 0;
	std::vector<wchar_t> outputBuffer(MAX_PATH + 1, 0);
	DWORD bytesRequired = MAX_PATH;
	for (;;) {
		if (::SetupDiGetDeviceProperty(deviceInfoSet, deviceInfoData, &DEVPKEY_Device_BusReportedDeviceDesc, &dataType,
			reinterpret_cast<PBYTE>(&outputBuffer[0]),
			bytesRequired, &bytesRequired, 0)) {
			break;
		}

		if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER
			|| (dataType != REG_SZ && dataType != REG_EXPAND_SZ)) {
			return std::wstring();
		}
		outputBuffer.resize(bytesRequired / sizeof(wchar_t) + 2, 0);
	}
	return std::wstring(outputBuffer.begin(), outputBuffer.end());
}

static std::wstring deviceDescription(HDEVINFO deviceInfoSet,
	PSP_DEVINFO_DATA deviceInfoData) {
	return deviceRegistryProperty(deviceInfoSet, deviceInfoData, SPDRP_DEVICEDESC);
}

static std::wstring deviceManufacturer(HDEVINFO deviceInfoSet,
	PSP_DEVINFO_DATA deviceInfoData) {
	return deviceRegistryProperty(deviceInfoSet, deviceInfoData, SPDRP_MFG);
}

static std::wstring deviceInstanceIdentifier(DEVINST deviceInstanceNumber) {
	std::vector<wchar_t> outputBuffer(MAX_DEVICE_ID_LEN + 1, 0);
	if (::CM_Get_Device_ID(
		deviceInstanceNumber,
		&outputBuffer[0],
		MAX_DEVICE_ID_LEN,
		0) != CR_SUCCESS) {
		return std::wstring();
	}
	return std::wstring(outputBuffer.begin(), outputBuffer.end());
}

static DEVINST parentDeviceInstanceNumber(DEVINST childDeviceInstanceNumber) {
	ULONG nodeStatus = 0;
	ULONG problemNumber = 0;
	if (::CM_Get_DevNode_Status(&nodeStatus, &problemNumber,
		childDeviceInstanceNumber, 0) != CR_SUCCESS) {
		return 0;
	}
	DEVINST parentInstanceNumber = 0;
	if (::CM_Get_Parent(&parentInstanceNumber, childDeviceInstanceNumber, 0) != CR_SUCCESS)
		return 0;
	return parentInstanceNumber;
}

static std::wstring parseDeviceSerialNumber(const std::wstring &instanceIdentifier) {
	int firstbound = instanceIdentifier.rfind('\\');
	int lastbound = instanceIdentifier.find('_', firstbound);
	if (instanceIdentifier.find(L"USB\\") == 0) {
		if (lastbound != instanceIdentifier.size() - 3)
			lastbound = instanceIdentifier.size();
		int ampersand = instanceIdentifier.find('&', firstbound);
		if (ampersand != -1 && ampersand < lastbound)
			return std::wstring();
	}
	else if (instanceIdentifier.find(L"FTDIBUS\\") == 0) {
		firstbound = instanceIdentifier.rfind('+');
		lastbound = instanceIdentifier.find('\\', firstbound);
		if (lastbound == -1)
			return std::wstring();
	}
	else {
		return std::wstring();
	}

	return instanceIdentifier.substr(firstbound + 1, lastbound - firstbound - 1);
}

static std::wstring deviceSerialNumber(std::wstring instanceIdentifier,
	DEVINST deviceInstanceNumber) {
	for (;;) {
		const std::wstring result = parseDeviceSerialNumber(instanceIdentifier);
		if (!result.empty())
			return result;
		deviceInstanceNumber = parentDeviceInstanceNumber(deviceInstanceNumber);
		if (deviceInstanceNumber == 0)
			break;
		instanceIdentifier = deviceInstanceIdentifier(deviceInstanceNumber);
		if (instanceIdentifier.empty())
			break;
	}

	return std::wstring();
}

static uint16_t parseDeviceIdentifier(const std::wstring &instanceIdentifier,
	const std::wstring &identifierPrefix,
	int identifierSize, bool &ok) {

	uint16_t result;
	const int index = instanceIdentifier.find(identifierPrefix);
	if (index == -1)
		return uint16_t(0);
	try
	{
		result = std::stoi(instanceIdentifier.substr(index + identifierPrefix.size(), identifierSize), nullptr, 16);
		ok = true;
	}
	catch (std::invalid_argument const & ex)
	{
		ok = false;
	}
	catch (std::out_of_range const & ex)
	{
		ok = false;
	}
	return result;
}

static uint16_t deviceVendorIdentifier(const std::wstring &instanceIdentifier, bool &ok) {
	static const int vendorIdentifierSize = 4;
	uint16_t result = parseDeviceIdentifier(
		instanceIdentifier, L"VID_", vendorIdentifierSize, ok);
	if (!ok)
		result = parseDeviceIdentifier(
			instanceIdentifier, L"VEN_", vendorIdentifierSize, ok);
	return result;
}

static uint16_t deviceProductIdentifier(const std::wstring &instanceIdentifier, bool &ok) {
	static const int productIdentifierSize = 4;
	uint16_t result = parseDeviceIdentifier(
		instanceIdentifier, L"PID_", productIdentifierSize, ok);
	if (!ok)
		result = parseDeviceIdentifier(
			instanceIdentifier, L"DEV_", productIdentifierSize, ok);
	return result;
}

static bool anyOfPorts(const std::vector<std::wstring> &ports, const std::wstring &portName) {
	const auto end = ports.end();
	auto isPortNamesEquals = [&portName](const std::wstring &port) {
		return port == portName;
	};
	return std::find_if(ports.begin(), end, isPortNamesEquals) != end;
}

static std::wstring devicePortName(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA deviceInfoData) {
	const HKEY key = ::SetupDiOpenDevRegKey(deviceInfoSet, deviceInfoData, DICS_FLAG_GLOBAL,
		0, DIREG_DEV, KEY_READ);
	if (key == INVALID_HANDLE_VALUE)
		return std::wstring();

	static const wchar_t * const keyTokens[] = {
		L"PortName\0",
		L"PortNumber\0"
	};

	enum { KeyTokensCount = sizeof(keyTokens) / sizeof(keyTokens[0]) };

	std::wstring portName;
	for (int i = 0; i < KeyTokensCount; ++i) {
		DWORD dataType = 0;
		std::vector<wchar_t> outputBuffer(MAX_PATH + 1, 0);
		DWORD bytesRequired = MAX_PATH;
		for (;;) {
			const LONG ret = ::RegQueryValueEx(key, keyTokens[i], nullptr, &dataType,
				reinterpret_cast<PBYTE>(&outputBuffer[0]), &bytesRequired);
			if (ret == ERROR_MORE_DATA) {
				outputBuffer.resize(bytesRequired / sizeof(wchar_t) + 2, 0);
				continue;
			}
			else if (ret == ERROR_SUCCESS) {
				if (dataType == REG_SZ)
					portName = std::wstring(outputBuffer.begin(), outputBuffer.end());
				else if (dataType == REG_DWORD)
					portName = L"COM" + std::to_wstring(DWORD(&outputBuffer[0]));
			}
			break;
		}

		if (!portName.empty())
			break;
	}
	::RegCloseKey(key);
	return portName;
}

// std::wstring to jstring
static jstring wstr2jstr(JNIEnv *env, std::wstring cstr) {
	jstring result = nullptr;
	try
	{
		int len = cstr.size();
		jchar* raw = new jchar[len];
		memcpy(raw, cstr.c_str(), len * sizeof(wchar_t));
		result = env->NewString(raw, len);
		delete[] raw;
		return result;
	}
	catch (const std::exception ex)
	{
		// TODO
		//std::wcout << L"EXCEPTION in wsz2jstr translating string input " << cstr << std::endl;
		//cout << "exception: " << ex.what() << std::endl;
	}

	return result;
}

static std::wstring jstr2wstr(JNIEnv *env, jstring string)
{
	std::wstring value;

	const jchar *raw = env->GetStringChars(string, 0);
	jsize len = env->GetStringLength(string);

	value.assign(raw, raw + len);

	env->ReleaseStringChars(string, raw);

	return value;
}

static std::wstring trimAfterFirstNull(std::wstring str) {
	size_t index = str.find_first_of(L'\0');
	if (index != std::string::npos) {
		str.erase(index, str.length());
	}
	return str;
}

static void addWStringToJavaArray(JNIEnv *env, jobjectArray objArray, int pos, std::wstring str) {
	jstring tmp = wstr2jstr(env, str);
	env->SetObjectArrayElement(objArray, pos, tmp);
	env->DeleteLocalRef(tmp);
}

template <typename I> std::wstring intToString(I num) {
	std::wstringstream sstream;
	sstream << std::hex << num;
	std::wstring result = sstream.str();
	return result;
}

static bool IsWindows7OrGreater()
{
	OSVERSIONINFOEXW osvi = { sizeof(osvi), 0, 0, 0, 0,{ 0 }, 0, 0 };
	DWORDLONG const dwlConditionMask = VerSetConditionMask(
		VerSetConditionMask(
			VerSetConditionMask(
				0, VER_MAJORVERSION, VER_GREATER_EQUAL),
			VER_MINORVERSION, VER_GREATER_EQUAL),
		VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);
	osvi.dwMajorVersion = HIBYTE(_WIN32_WINNT_WIN7);
	osvi.dwMinorVersion = LOBYTE(_WIN32_WINNT_WIN7);
	osvi.wServicePackMajor = 0;

	return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}
