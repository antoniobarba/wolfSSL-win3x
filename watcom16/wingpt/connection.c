#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winsock.h>

#include <wolfssl/wolfcrypt/user_settings.h>
#undef WOLFSSL_16BIT_API
#define WOLFSSL_16BIT_API __far __pascal
#include <wolfssl/error-ssl.h>
#include <wolfssl/ssl.h>

#include "jsmn.h"
#include "utils.h"

#define MAKEWORD(a,b) ((WORD)(((BYTE)(a))|(((WORD)((BYTE)(b)))<<8)))
#define GlobalAllocPtr(flags, cb) \
    (GlobalLock(GlobalAlloc((flags), (cb))))
#define GlobalFreePtr(lp) \
    (GlobalUnlock((HGLOBAL)lp), GlobalFree(GlobalHandle((HGLOBAL)lp)))

#define BUFF_SIZE 10000
#define API_DOMAIN "api.openai.com"

#pragma off(unreferenced)
void wolfLogger(const int logLevel, const char *const logMessage) {
#pragma on(unreferenced)
    MessageBox(
        0,
        logMessage,
        "Error",
        MB_OK | MB_TASKMODAL | MB_ICONINFORMATION
    );
}

int ErrorBox(LPCSTR lpBody) {
    return MessageBox(
        0,
        lpBody,
        "Error",
        MB_OK | MB_TASKMODAL | MB_ICONSTOP
    );
}

#pragma off(unreferenced)
int my_IORecv(WOLFSSL* ssl, char* buff, int sz, void* ctx)
#pragma on(unreferenced)
{
    int sockfd = *(int*)ctx;
    int recvd;

    /* Receive message from socket */
    if ((recvd = recv(sockfd, buff, sz, 0)) == -1) {
        switch (errno) {
            case WSAEWOULDBLOCK:
                return WOLFSSL_CBIO_ERR_TIMEOUT;
            case WSAECONNRESET:
                return WOLFSSL_CBIO_ERR_CONN_RST;
            case WSAEINTR:
                return WOLFSSL_CBIO_ERR_ISR;
            case WSAECONNREFUSED:
                return WOLFSSL_CBIO_ERR_WANT_READ;
            case WSAECONNABORTED:
                return WOLFSSL_CBIO_ERR_CONN_CLOSE;
            default:
                return WOLFSSL_CBIO_ERR_GENERAL;
        }
    } else if (recvd == 0) {
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }

    // Successful receive
    return recvd;
}

#pragma off(unreferenced)
int my_IOSend(WOLFSSL* ssl, char* buff, int sz, void* ctx)
#pragma on(unreferenced)
{
    int sockfd = *(int*)ctx;
    int sent;

    /* Receive message from socket */
    if ((sent = send(sockfd, buff, sz, 0)) == -1) {
        switch (errno) {
            case WSAEWOULDBLOCK:
                return WOLFSSL_CBIO_ERR_WANT_WRITE;
            case WSAECONNRESET:
                return WOLFSSL_CBIO_ERR_CONN_RST;
            case WSAEINTR:
                return WOLFSSL_CBIO_ERR_ISR;
            default:
                return WOLFSSL_CBIO_ERR_GENERAL;
        }
    } else if (sent == 0) {
        // Connection closed
        return 0;
    }

    /* successful send */
    return sent;
}

void DestructivelyUnescapeStr(LPSTR lpInput) {
    int offset = 0;
    int i = 0;
    while (lpInput[i] != '\0') {
        if (lpInput[i] == '\\') {
            offset++;
        } else {
            lpInput[i - offset] = lpInput[i];
        }
        i++;
    }
    lpInput[i - offset] = '\0';
}

void ParseJSONResponse(char *buff, int buffSz, HWND hwndChatLogEdit) {
    jsmn_parser parser;
    jsmntok_t tokens[50];
    int retVal;
    LPSTR JSONStart;
    int i;

    JSONStart = strstr(buff, "\r\n\r\n");
    if (!JSONStart) {
        ErrorBox("Could not find beginning of JSON");
        return;
    }
    JSONStart += 4;

    jsmn_init(&parser);

    retVal = jsmn_parse(&parser, JSONStart, buffSz - (JSONStart - buff), tokens, 50);
    if (retVal < 0) {
        switch (retVal) {
            case JSMN_ERROR_INVAL:
                ErrorBox("JSON string is corrupted");
                break;
            case JSMN_ERROR_NOMEM:
                ErrorBox("Not enough tokens, JSON string is too large");
                break;
            case JSMN_ERROR_PART:
                ErrorBox("JSON string is too short");
                break;
        }
        return;
    }

    for (i = 0; i < retVal; i++) {
        // See if this is the "content" token
        if (tokens[i].type == JSMN_STRING && tokens[i].size == 1 &&
            JSONStart[tokens[i].start] == 'c' && JSONStart[tokens[i].end - 1] == 't' &&
            tokens[i].end - tokens[i].start == 7) {
            // Next token is the message
            JSONStart[tokens[i + 1].end] = '\0';

            AppendTextToEditControl(hwndChatLogEdit, "WinGPT: ");
            DestructivelyUnescapeStr(&JSONStart[tokens[i + 1].start]);
            AppendTextToEditControl(hwndChatLogEdit, &JSONStart[tokens[i + 1].start]);
            AppendTextToEditControl(hwndChatLogEdit, "\r\n\r\n");

            break;
        }
    }
}

int ProcessAssistantQuery(HWND hwndChatLogEdit, LPCSTR inputText, char* apiKey) {
    int ret = 0;
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    SOCKET conn;
    SOCKADDR_IN servAddr;
    LPSTR lpRequestBody;
    int requestBodySz;
    LPSTR lpBuff;
    int buffOffset;
    int buffChars;
    int iResult;
    WOLFSSL_CTX* ctx;
    WOLFSSL* ssl;
    int iErrCode;

    wVersionRequested = MAKEWORD(1, 1);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err != 0) {
        ErrorBox("Can't initialize Winsock");
        return err;
    }

    conn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn == INVALID_SOCKET) {
        ErrorBox("Got invalid socket back");
        ret = -1;
        goto end;
    }

    memset(&servAddr, 0, sizeof(servAddr));

    /* Fill in the server address */
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(443);
    servAddr.sin_addr.s_addr = inet_addr("104.18.6.192");

    /* Connect to the server */
    if ((ret = connect(conn, (LPSOCKADDR) &servAddr, sizeof(servAddr)))
         == SOCKET_ERROR) {
        ErrorBox("Failed to connect");
        ret = -1;
        goto end;
    }

    /* Initialize WolfSSL */
    if ((ret = wolfSSL_Init()) != WOLFSSL_SUCCESS) {
        ErrorBox("Failed to initialize WolfSSL");
        goto socket_cleanup;
    }

    /* Create and initialize WOLFSSL_CTX */
    if ((ctx = wolfSSL_CTX_new(wolfTLS_client_method())) == NULL) {
        ErrorBox("ERROR: failed to create WOLFSSL_CTX");
        ret = -1;
        goto socket_cleanup;
    }

    /* Don't check certificate */
    wolfSSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);

    /* Create a WOLFSSL object */
    if ((ssl = wolfSSL_new(ctx)) == NULL) {
        ErrorBox("ERROR: failed to create WOLFSSL object");
        ret = -1;
        goto ctx_cleanup;
    }

    ret = wolfSSL_UseSNI(ssl, WOLFSSL_SNI_HOST_NAME, API_DOMAIN, strlen(API_DOMAIN));
    if (ret != WOLFSSL_SUCCESS) {
        goto ctx_cleanup;
    }

    /* Attach wolfSSL to the socket */
    wolfSSL_SetIOReadCtx(ssl, &conn);
    wolfSSL_CTX_SetIORecv(ctx, my_IORecv);
    wolfSSL_SSLSetIORecv(ssl, my_IORecv);
    wolfSSL_SetIOWriteCtx(ssl, &conn);
    wolfSSL_CTX_SetIOSend(ctx, my_IOSend);
    wolfSSL_SSLSetIOSend(ssl, my_IOSend);

/*
    // Enable debugging
    wolfSSL_SetLoggingCb(wolfLogger);
    wolfSSL_Debugging_ON();
*/

    /* Prepare buff - must goto cleanup after this */
    lpBuff = GlobalAllocPtr(GMEM_MOVEABLE, BUFF_SIZE);

    /* Connect to wolfSSL on the server side */
    if ((ret = wolfSSL_connect(ssl)) != SSL_SUCCESS) {
        iErrCode = wolfSSL_get_error(ssl, ret);
        sprintf(lpBuff, "ERROR: failed to connect wolfSSL: Error Code %d", iErrCode);
        ErrorBox(lpBuff);
        goto cleanup;
    }

    /* Prepare the buffer */

    lpRequestBody = GlobalAllocPtr(GMEM_MOVEABLE, BUFF_SIZE);
    requestBodySz = sprintf(
        lpRequestBody,
        "{\"model\":\"gpt-3.5-turbo\",\"messages\":[{\"role\":\"system\",\"content\":\""
        "You are a friendly but concise chat bot. Answer all questions in the present tense as if it is 1992, "
        "and make no reference to the current year.\"},{\"role\":\"user\",\"content\":\"%s"
        "\"}],\"temperature\":0.7}",
        inputText
    );

    buffChars = sprintf(
        lpBuff,
        "POST /v1/chat/completions HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\n"
        "Content-Length:%d\r\nAuthorization: Bearer %s\r\nConnection: close\r\n\r\n%s",
        API_DOMAIN,
        requestBodySz,
        apiKey,
        lpRequestBody
    );
    GlobalFreePtr(lpRequestBody);

    /* Send the message to the server */
    if (wolfSSL_write(ssl, lpBuff, buffChars) != buffChars) {
        ErrorBox("Failed to write");
        ret = -1;
        goto cleanup;
    }

    /* Read the server data into our array */
    buffOffset = 0;
    do {
        iResult = wolfSSL_read(ssl, (lpBuff + buffOffset), BUFF_SIZE - 1);
        if (iResult <= 0) {
            iErrCode = wolfSSL_get_error(ssl, iResult);
            if (iErrCode != ZERO_RETURN && iErrCode != SOCKET_PEER_CLOSED_E &&
                iErrCode != WOLFSSL_ERROR_ZERO_RETURN) {
                // If close notify or peer closed socket, then it's not an error
                sprintf(lpBuff, "Failed to read; error code: %d", iErrCode);
                ErrorBox(lpBuff);
                ret = -1;
                goto cleanup;
            }
        } else {
            buffOffset += iResult;
        }
    } while (iResult > 0);

    // Done!
    // Insert null terminator
    lpBuff[buffOffset] = '\0';
    ParseJSONResponse(lpBuff, buffOffset, hwndChatLogEdit);

    while (wolfSSL_shutdown(ssl) == SSL_SHUTDOWN_NOT_DONE) {}

    ret = 0;


cleanup:
    GlobalFreePtr(lpBuff);
    wolfSSL_free(ssl);
ctx_cleanup:
    wolfSSL_CTX_free(ctx);
    wolfSSL_Cleanup();
socket_cleanup:
    closesocket(conn);
end:
    WSACleanup();
    return ret;
}
