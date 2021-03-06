#include "pch.h"
//#define _CRT_SECURE_NO_WARNINGS
#include "tls.h"
//#include "utils.h"
#include <stdarg.h>



void info_printf(FILE* fp, const char* format, ...)
{
	if (fp == NULL) {
		va_list argp;
		va_start(argp, format); vprintf(format, argp); va_end(argp);
	}
	else {
		va_list arg;
		va_start(arg, format); vfprintf(fp, format, arg); va_end(arg);

	}
}

int initialize_tls_structure(unsigned char* raw, int size, HandshakeMessage* tls_message, HandshakeMessage* tls_message2, int* nextSize) {
	// Record layer

	if (size <= MIN_RECORD_LAYER_SIZE || raw == NULL) {
		return INVALID_FILE_LENGTH;
	}

	//suppose there is no tls_message2
	tls_message2->mLength = 0;

	int pos = 0;
	myContentType = raw[pos++];//fg
	// Only handshake messages of TLS version 1.0 - 1.2 are allowed
	if (myContentType != CHANGE_CIPHER_SPEC && myContentType != ALERT && myContentType != HANDSHAKE && myContentType != APPLICATION_DATA) {
		printf("INVALID_CONTENT_TYPE: %d\n", myContentType);//fg

		return INVALID_CONTENT_TYPE; //2
	}

	if (!is_valid_tls_version(raw[pos], raw[pos + 1])) {
		return INVALID_VERSION;
	}

	// Values are safe to assign to our structure
	if (myContentType == CHANGE_CIPHER_SPEC) {
		tls_message->cType = (ContentType)CHANGE_CIPHER_SPEC;
	}
	else if (myContentType == ALERT) {
		tls_message->cType = ALERT;
	}
	else if (myContentType == HANDSHAKE) {
		tls_message->cType = HANDSHAKE;
	}
	else {
		tls_message->cType = APPLICATION_DATA;
	}

	tls_message->version.major = raw[1];
	tls_message->version.minor = raw[2];

	pos += 2;

	// Convert raw[3] and raw[4] to uint16_t number
	tls_message->fLength = (raw[pos] << 8) + raw[pos + 1];
	pos += 2;

	// Check if the sizes are correct (record protocol headers + length == file size)
	if (tls_message->fLength + pos > size) {
		//printf("[debug]: not end :tls_message->fLength = %d , pos = %d, but size = %d\n", tls_message->fLength, pos, size); //fg
		return NEED_MORE;
	}
	//sometimes there are few msgs in one tcp packet
	if (tls_message->fLength + pos < size) {
		(*nextSize) = size - (tls_message->fLength + pos);
	}

	//?
	if (tls_message->cType == HANDSHAKE && tls_message->fLength <= 40) { 
		tls_message->cType = ENCRYPTED_HANDSHAKE;
	}

	if (tls_message->cType == HANDSHAKE) { //fg
		int t = raw[pos++];
		if (t >= 0 && t <= 20) {

			tls_message->hsType = (HandshakeType)t;

			// Convert raw[6], raw[7] and raw[8] into uint24_t number
			// It's actually uint24_t but thats not defined
			tls_message->mLength = (0x00 << 24) + (raw[pos] << 16) + (raw[pos + 1] << 8) + raw[pos + 2];
			pos += 3;

			// Check if the sizes are correct (fLength value == mLength value + HandshakeType (1 byte) + mLength (3 bytes))
			//maybe there is the second handshake message
			if (tls_message->fLength > tls_message->mLength + 4) {
				if (tls_message->fLength != tls_message->mLength + 4 + 4) {
					return INVALID_FILE_LENGTH;
				}
				tls_message2->mLength = 0;
				tls_message2->cType = HANDSHAKE;
				tls_message2->version.major = raw[1];
				tls_message2->version.minor = raw[2];
				tls_message2->fLength = tls_message->fLength;
				tls_message2->hsType = (HandshakeType)raw[pos + tls_message->mLength];
				if (tls_message2->hsType != SERVER_HELLO_DONE)//14;
					printf("second tls_message is not SERVER_HELLO_DONE\n");
				//tls_message2->hsType = SERVER_HELLO_DONE; //

			}
			else if (tls_message->fLength != tls_message->mLength + 4) {
				//return INVALID_FILE_LENGTH;
				tls_message->hsType = ENCRYPTED_HANDSHAKE_MESSAGE;
				tls_message->mLength = tls_message->fLength;
			}
		}
		else {
			tls_message->hsType = ENCRYPTED_HANDSHAKE_MESSAGE;
			tls_message->mLength = tls_message->fLength;
		}
	}
	else {
		tls_message->mLength = tls_message->fLength;
	}

	// Copy the rest of the message into our structure, so we can close the raw stream
	tls_message->body = (unsigned char*)malloc(tls_message->mLength);
	memcpy(tls_message->body, raw + pos, tls_message->mLength);

	return 0;
}

void print_tls_record_layer_info(FILE* fp, HandshakeMessage* tls_message) {
	if (tls_message->version.major == 1)
		info_printf(fp, "Identified the following GMTLS message:\n\n");
	else if (tls_message->version.major == 3)
		info_printf(fp, "Identified the following TLS message:\n\n");
	else
		info_printf(fp, "error:wrong major version\n\n");

	info_printf(fp, "Version: ");

	print_tls_version(fp, tls_message->version.minor);

	info_printf(fp, "Protocol type: %d\n", tls_message->cType);
	info_printf(fp, "Fragment length: %d\n", tls_message->fLength);
	if (tls_message->cType == HANDSHAKE) {
		info_printf(fp, "Handshake message type: %d\n", tls_message->hsType);
	}
	info_printf(fp, "\n");
}

int parse_application_data(FILE* fp, unsigned char* message, uint16_t size) {
	if (message == NULL) {
		return INVALID_FILE_LENGTH;
	}
	//printf("Encrypted data of Application_Data:\n");
	//for (int i = 0; i < size; i++) {
	//	//info_printf(fp, "%x", message[i]);
	//	
	//}
	fwrite(message, sizeof(unsigned char), size, fp);
	//printf("\n");
	return 0;
}
int parse_other_ctype(FILE* fp, unsigned char* message, uint16_t size) {
	if (message == NULL) {
		return INVALID_FILE_LENGTH;
	}
	//printf("Encrypted data of Application_Data:\n");
	for (int i = 0; i < size; i++) {
		info_printf(fp, "%x", message[i]);
	}
	//printf("\n");
	return 0;
}
int parse_client_hello(FILE* fp, unsigned char* message, uint16_t size) {
	if (size < MIN_CLIENT_HELLO_SIZE || message == NULL) {
		return INVALID_FILE_LENGTH;
	}

	int pos = 0;

	ClientHello client_hello;
	memset(&client_hello, 0, sizeof(client_hello));

	// Check if the versions are valid
	if (!is_valid_tls_version(message[pos], message[pos + 1])) {
		return INVALID_VERSION;
	}

	client_hello.version.major = message[pos];
	client_hello.version.minor = message[pos + 1];
	pos += 2;

	// The Random structure    
	client_hello.random.time = (message[pos] << 24) + (message[pos + 1] << 16) + (message[pos + 2] << 8) + message[pos + 3];
	pos += 4;
	memcpy(client_hello.random.random_bytes, message + pos, HELLO_RANDOM_BYTES_SIZE);
	pos += HELLO_RANDOM_BYTES_SIZE;

	// The SessionID structure
	client_hello.sessionId.length = message[pos++];
	if (client_hello.sessionId.length > 0) {
		if (size < client_hello.sessionId.length + HELLO_RANDOM_BYTES_SIZE + 4 + 2) {
			return INVALID_FILE_LENGTH;
		}

		client_hello.sessionId.sessionId = (unsigned char*)malloc(client_hello.sessionId.length);
		memcpy(client_hello.sessionId.sessionId, message + pos, client_hello.sessionId.length);
		pos += client_hello.sessionId.length;
	}

	// The CipherSuitesStructure
	client_hello.csCollection.length = (message[pos] << 8) + message[pos + 1];
	pos += 2;
	if (client_hello.csCollection.length > 0) {
		if (size < pos + client_hello.csCollection.length) {
			return INVALID_FILE_LENGTH;
		}

		client_hello.csCollection.cipherSuites = (unsigned char*)malloc(client_hello.csCollection.length);
		memcpy(client_hello.csCollection.cipherSuites, message + pos, client_hello.csCollection.length);
		pos += client_hello.csCollection.length;
	}

	// CompresionMethod 1 bytes and Extensions 1 at least
	if (size < pos + 2) {
		return INVALID_FILE_LENGTH;
	}

	// The CompresionMethodStructure
	client_hello.compresionMethod.length = message[pos++];
	if (client_hello.compresionMethod.length != 1) {
		info_printf(fp, "%x", client_hello.compresionMethod.length);
		return INVALID_FILE_LENGTH;
	}

	client_hello.compresionMethod.compresionMethod = message[pos++];

	if (size != pos) {
		// Extensions are present.
		// Save to rest of the data to our structue. No more checks about it,
		// we will just print it out as extensions are not in scope. 
		client_hello.hasExtensions = 1;
		client_hello.extensions = (unsigned char*)malloc(size - pos);
		memcpy(client_hello.extensions, message + pos, size - pos);
	}

	print_client_hello_message(fp, &client_hello, size - pos);

	clean_client_hello(client_hello);

	return 0;
}

void print_client_hello_message(FILE* fp, ClientHello* message, int extensions_length) {
	info_printf(fp, "Details of ClientHello:\n\n");
	info_printf(fp, "TLS Version: ");

	print_tls_version(fp, message->version.minor);

	// Time in human-readable format
	time_t raw_time = (time_t)message->random.time;
	struct tm* timeinfo = localtime(&raw_time);
	char buf[25];

	strftime(buf, 25, "Timestamp: %c.", timeinfo);
	if (fp == NULL)
		puts(buf);
	else
		fputs(buf, fp);

	info_printf(fp, "Random data: ");
	int i;
	for (i = 0; i < HELLO_RANDOM_BYTES_SIZE; i++) {
		info_printf(fp, "%x", message->random.random_bytes[i]);
	}
	info_printf(fp, "\n");

	info_printf(fp, "SessionID: ");
	if (message->sessionId.length != 0) {
		for (i = 0; i < message->sessionId.length; i++) {
			info_printf(fp, "%x", message->sessionId.sessionId[i]);
		}
	}
	else {
		info_printf(fp, "N/A");
	}

	info_printf(fp, "\n");

	info_printf(fp, "Choosen cipher suites:\n");
	for (i = 0; i < message->csCollection.length; i++) {
		if (i % 2) {
			info_printf(fp, "%x ", message->csCollection.cipherSuites[i]);
		}
		else {
			info_printf(fp, "0x%x", message->csCollection.cipherSuites[i]);
		}
	}
	info_printf(fp, "\n");

	info_printf(fp, "Compresion method: %d\n", message->compresionMethod.compresionMethod);
	info_printf(fp, "Has extensions: %s\n", message->hasExtensions ? "true" : "false");

	if (message->hasExtensions) {
		info_printf(fp, "Raw extensions data:\n");
		for (i = 0; i < extensions_length; i++) {
			info_printf(fp, "%x", message->extensions[i]);
		}
		info_printf(fp, "\n");
	}
}

int parse_server_hello(FILE* fp, unsigned char* message, uint16_t size) {
	if (size < MIN_SERVER_HELLO_SIZE || message == NULL) {
		return INVALID_FILE_LENGTH;
	}

	int pos = 0;

	ServerHello server_hello;
	memset(&server_hello, 0, sizeof(server_hello));

	// Check if the versions are valid
	if (!is_valid_tls_version(message[pos], message[pos + 1])) {
		return INVALID_VERSION;
	}

	server_hello.version.major = message[pos];
	server_hello.version.minor = message[pos + 1];
	pos += 2;

	// The Random structure    
	server_hello.random.time = (message[pos] << 24) + (message[pos + 1] << 16) + (message[pos + 2] << 8) + message[pos + 3];
	pos += 4;
	memcpy(server_hello.random.random_bytes, message + pos, HELLO_RANDOM_BYTES_SIZE);
	pos += HELLO_RANDOM_BYTES_SIZE;

	// The SessionID structure
	server_hello.sessionId.length = message[pos++];
	if (server_hello.sessionId.length > 0) {
		if (size < server_hello.sessionId.length + HELLO_RANDOM_BYTES_SIZE + 4 + 2) {
			return INVALID_FILE_LENGTH;
		}

		server_hello.sessionId.sessionId = (unsigned char*)malloc(server_hello.sessionId.length);
		memcpy(server_hello.sessionId.sessionId, message + pos, server_hello.sessionId.length);
		pos += server_hello.sessionId.length;
	}

	// The choosen cipher suite
	server_hello.cipherSuite[0] = message[pos++];
	server_hello.cipherSuite[1] = message[pos++];

	// CompresionMethod needs to be present
	if (size < pos + 1) {
		return INVALID_FILE_LENGTH;
	}

	// The CompresionMethodStructure
	server_hello.compresionMethod = message[pos++];

	if (size != pos) {
		// Extensions are present.
		// Save to rest of the data to our structue. No more checks about it,
		// we will just print it out as extensions are not in scope. 
		server_hello.hasExtensions = 1;
		server_hello.extensions = (unsigned char*)malloc(size - pos);
		memcpy(server_hello.extensions, message + pos, size - pos);
	}

	print_server_hello_message(fp, &server_hello, size - pos);

	clean_server_hello(server_hello);

	return 0;
}

void print_server_hello_message(FILE* fp, ServerHello* message, int extensions_length) {
	info_printf(fp, "Details of ServerHello:\n\n");
	info_printf(fp, "TLS Version: ");

	print_tls_version(fp, message->version.minor);

	// Time in human-readable format
	time_t raw_time = (time_t)message->random.time;
	struct tm* timeinfo = localtime(&raw_time);
	info_printf(fp, "Timestamp: %s", asctime(timeinfo));

	info_printf(fp, "Random data: ");
	int i;
	for (i = 0; i < HELLO_RANDOM_BYTES_SIZE; i++) {
		info_printf(fp, "%x", message->random.random_bytes[i]);
	}
	info_printf(fp, "\n");

	info_printf(fp, "SessionID: ");
	if (message->sessionId.length != 0) {
		for (i = 0; i < message->sessionId.length; i++) {
			info_printf(fp, "%x", message->sessionId.sessionId[i]);
		}
	}
	else {
		info_printf(fp, "N/A");
	}

	info_printf(fp, "\n");

	info_printf(fp, "Choosen cipher suite: 0x");
	info_printf(fp, "%x", message->cipherSuite[0]);
	info_printf(fp, "%x\n", message->cipherSuite[1]);

	info_printf(fp, "Compresion method: %d\n", message->compresionMethod);
	if (message->hasExtensions) {
		info_printf(fp, "Has extensions: true\n");
		info_printf(fp, "Raw extensions data:\n");
		for (i = 0; i < extensions_length; i++) {
			info_printf(fp, "%x", message->extensions[i]);
		}
	}
	else {
		info_printf(fp, "Has extensions: false");
	}

	info_printf(fp, "\n");
}


void print_tls_version(FILE* fp, uint8_t minor) {
	switch (minor) {
	case 0x01: info_printf(fp, "1.0\n"); break;
	case 0x02: info_printf(fp, "1.1\n"); break;
	case 0x03: info_printf(fp, "1.2\n"); break;
	default: info_printf(fp, "unknown\n"); break;
	}
}

int parse_certificate(FILE* fp, uint16_t size) {
	// The Certificate message contains only a chain of certificates. 
	// The only thing to do is to verify, that the chain is not empty 
	// as we are not able to (and not supposed to) say anything about the data.
	if (size == 0) {
		return INVALID_FILE_LENGTH;
	}

	info_printf(fp, "The certificate chain provided is %d bytes long.\n", size);

	return 0;
}

//todo
int parse_certificate_request(FILE* fp, uint16_t size) {
	
	if (size == 0) {
		return INVALID_FILE_LENGTH;
	}

	info_printf(fp, "Certificate_Request.\n", size);

	return 0;
}

//todo
int parse_certificate_verify(FILE* fp, uint16_t size) {

	if (size == 0) {
		return INVALID_FILE_LENGTH;
	}

	info_printf(fp, "Certificate_Request.\n", size);

	return 0;
}


int parse_server_key_exchange(FILE* fp, uint16_t size) {
	// The actual algorithm and other stuff like digital signatures of params
	// are not in scope as their presence is determined by extensions in hello messages
	// and the used certificate (which are both ignored).

	info_printf(fp, "The key exchange parameters provided are %d bytes long.\n", size);

	return 0;
}

int parse_server_hello_done(FILE* fp, uint16_t size) {
	// The ServerHelloDone is empty. Just check if thats true.
	if (size != 0) {
		return INVALID_FILE_LENGTH;
	}

	info_printf(fp, "Server_Hello_Done.\n");
	return 0;
}

int parse_client_key_exchange(FILE* fp, unsigned char* message, uint16_t size) {
	// We only check until we get to the exchange parameters, whose
	// type is specified similiary as server key exchange parameters
	// in earlier messages.

	/*uint8_t length = message[0];
	if (length != size - 1) {
		return INVALID_FILE_LENGTH_FOR_CLIENT_KEY_EXCHANGE;
	}*/

	info_printf(fp, "The key exchange parameters provided are %d bytes long.\n", size);

	return 0;
}

//TODO:
int parse_change_cipher_spec(FILE* fp, unsigned char* message, uint16_t size) {
	//TODO:

	info_printf(fp, "parse_client_key_exchange��size:%d.\n", size);

	return 0;
}
//TODO:
int parse_alert(FILE* fp, unsigned char* message, uint16_t size) {

	info_printf(fp, "Alert");

	if (size >= 2) {

		info_printf(fp, "��level = %d", message[0]);
		info_printf(fp, ",description = %d", message[1]);

	}
	info_printf(fp, "\n");

	return 0;
}
void clean_client_hello(ClientHello message) {
	if (message.sessionId.sessionId) {
		free(message.sessionId.sessionId);
	}

	if (message.csCollection.cipherSuites) {
		free(message.csCollection.cipherSuites);
	}

	if (message.extensions) {
		free(message.extensions);
	}
}

void clean_server_hello(ServerHello message) {
	if (message.sessionId.sessionId) {
		free(message.sessionId.sessionId);
	}

	if (message.extensions) {
		free(message.extensions);
	}
}

int is_valid_tls_version(unsigned char major, unsigned char minor) {
	bool a = (major == 0x03 && (minor == 0x01 || minor == 0x02 || minor == 0x03));
	if (a)
		return a;
	bool b = (major == 0x01 && (minor == 0x01));
	return b;
}

int tls_version_type(unsigned char major, unsigned char minor) {
	if (major == 0x03 && (minor == 0x01 || minor == 0x02 || minor == 0x03))
		return TLS;

	if (major == 0x01 && (minor == 0x01))
		return GMTLS;

	return NONTLS;
}


unsigned char* get_safe_input_file(char* path, int* file_size) {


#ifdef LINUX  //fg
	struct stat sb;
	// Only regular files are processed. No symbolic links, sockets, dirs, etc.
	if (lstat(path, &sb) == 0 && !S_ISREG(sb.st_mode))
	{
		printf("The path '%s' is not a regular file.\n", path);

		return NULL;
	}
#endif

	// Try to open provided file
	FILE* stream;
	stream = fopen(path, "rb");

	if (stream == NULL) {
		printf("The file '%s' couldn't be opened.\n", path);
		return NULL;
	}

	// Get the actual file length
	if (fseek(stream, 0, SEEK_END) != 0) {    //fg:fseeko
		printf("Couldn't read the file '%s' (fseeko).\n", path);
		fclose_safe(stream);

		return NULL;
	}

	*file_size = ftell(stream); //fg:ftello
	if (*file_size == -1) {
		printf("Couldn't read the file '%s' (ftello).\n", path);
		fclose_safe(stream);

		return NULL;
	}

	// Prevent hangs when a very large file is given by the user
	if (*file_size > MAXIMUM_FILE_SIZE) {
		printf("The file '%s' is larger then 20 MB.\n", path);
		fclose_safe(stream);

		return NULL;
	}

	fseek(stream, 0, SEEK_SET);

	// Copy file content into buffer and close file stream
	unsigned char* buf = (unsigned char*)malloc(*file_size);
	fread(buf, *file_size, 1, stream);

	fclose_safe(stream);

	return buf;
}

void fclose_safe(FILE* stream) {
	if (stream != NULL) {
		fclose(stream);
	}
}

void handle_errors(int error_code) {
	if (!error_code) {
		// In case there is no error, continue.
		return;
	}

	printf("[ERROR]: ");

	switch (error_code) {
	case 1: // INVALID_FILE_LENGTH
		printf("The lengths specified in the input file are not valid.\n"); break;
	case 2: //	INVALID_CONTENT_TYPE
		printf("The input file is not an TLS handshake message.\n");
		break;
	case 3: printf("The message is not of a supported version (TLS 1.0 - TLS 1.2).\n"); break;
	case 4: //UNSUPPORTED_MESSAGE_TYPE
		printf("Unsupported handshake message type.\n"); break;
	case 5: //INVALID_FILE_LENGTH_FOR_CLIENT_KEY_EXCHANGE
		printf("The lengths specified in the input file are not valid for client_key_exchange message.\n"); break;
	case 6: //NEED_MORE
		printf("handle_errors: need more.\n"); break;
	default:
		printf("Something truly unexpected happend.\n"); break;
	}

	//exit(0);
}

int handleTLSPacket(unsigned char* buf, int file_size, FILE* out_fd, int debug) {
	static int count = 0;
	count++;
	int err, type = 0;
	// Parse the record layer headers and save the actual handshake message into tls_message->body
	HandshakeMessage tls_message;
	HandshakeMessage tls_message2; //sometimes there is the second tls message.
	memset(&tls_message, 0, sizeof(tls_message));
	int nextSize = 0;

	if (debug)
		out_fd = NULL;

	err = initialize_tls_structure(buf, file_size, &tls_message, &tls_message2, &nextSize);

	// Close the original buffer containing the file stream, as all data has to be in tls_message
	/*if (buf) {
		free(buf);
	}*/

	// Stop processing in case there was an error
	if (err == NEED_MORE)
		return err;
	if (debug)
		handle_errors(err);
	if (err == INVALID_CONTENT_TYPE)
		return(err);

	info_printf(out_fd, "---------------------------------------\n");
	print_tls_record_layer_info(out_fd, &tls_message);


	if (tls_message.cType == APPLICATION_DATA) {
		//err = parse_application_data(out_fd, tls_message.body, tls_message.fLength);
		//temporarily do nothing
		err = 0;
	}
	else if (tls_message.cType == CHANGE_CIPHER_SPEC) {

		err = parse_change_cipher_spec(out_fd, tls_message.body, tls_message.fLength);

	}
	else if (tls_message.cType == ALERT) {


		err = parse_alert(out_fd, tls_message.body, tls_message.fLength);

	}
	else if (tls_message.cType == ENCRYPTED_HANDSHAKE) {
		//temporarily do nothing

	}
	else {
		switch (tls_message.hsType) {
		case 1:
			err = parse_client_hello(out_fd, tls_message.body, tls_message.mLength); break;
		case 2:
			err = parse_server_hello(out_fd, tls_message.body, tls_message.mLength); break;
		case 11:
			err = parse_certificate(out_fd, tls_message.mLength); break;
		case 12:
			err = parse_server_key_exchange(out_fd, tls_message.mLength); break;
		case 13:
			err = parse_certificate_request(out_fd, tls_message.mLength); break;  //todo
		case 14:
			err = parse_server_hello_done(out_fd, tls_message.mLength); break;
		case 15:
			err = parse_certificate_verify(out_fd, tls_message.mLength); break;
		case 16:
			err = parse_client_key_exchange(out_fd, tls_message.body, tls_message.mLength); break;
		case 99:
			info_printf(out_fd, "ENCRYPTED_HANDSHAKE_MESSAGE\n"); break;
		default:
			err = UNSUPPORTED_MESSAGE_TYPE; break;
		}
	}
	if (tls_message2.hsType == SERVER_HELLO_DONE) {
		err = parse_server_hello_done(out_fd, tls_message2.mLength);
	}

	// Process the actual handshake message

	if (tls_message.body) {
		free(tls_message.body);
	}
	handle_errors(err);

	if (nextSize > 0 && err == 0) {
		//printf("file_size = %d, nextSize = %d\n", file_size, nextSize);
		count--;
		err = handleTLSPacket(buf + ((file_size - nextSize) * sizeof(unsigned char)), nextSize, out_fd, debug);
	}

	return err;
}
