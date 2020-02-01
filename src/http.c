#ifndef HTTP
#define HTTP

#include <stdio.h>
#include <string.h>

#define TCP_ON_NEW_CONNETION_CALLBACK HttpTcpOnNewConnection
#define TCP_ON_CONNECT_CALLBACK HttpTcpOnConnect
#define TCP_ON_INCOMING_DATA_CALLBACK HttpTcpOnIncomingData
#define TCP_ON_DISCONNECT_CALLBACK HttpTcpOnDisconnect

#include "tcp.c"

#ifndef HTTP_TCP_INCLUDED
#define HTTP_TCP_INCLUDED 0
#endif

#define HTTP_MAX_METHOD_LENGTH 10

#define HTTP_REQUEST_STATE_NO_REQUEST 0
#define HTTP_REQUEST_STATE_START_REQUEST 1
#define HTTP_REQUEST_STATE_METHOD 2
#define HTTP_REQUEST_STATE_URL 3
#define HTTP_REQUEST_STATE_VERSION 4

#define HTTP_STATE_LINUX_END_HEADER 5
#define HTTP_STATE_MAC_END_HEADER 6
#define HTTP_STATE_WIN_END_HEADER 7
#define HTTP_STATE_WIN_END_HEADER2 8
#define HTTP_STATE_HEADER 9
#define HTTP_STATE_END_HEADER 10
#define HTTP_STATE_END_MESSAGE 11

#define HTTP_REQUEST_STATE_REQUEST_HANDLING 12

#ifndef HTTP_HEADER_ROW_BREAK
#define HTTP_HEADER_ROW_BREAK "\r\n"
#endif

typedef struct{
 unsigned short headersLenght;
 unsigned char headers[HTTP_MAX_HEADER_ROWS_LENGTH+1];
 unsigned char data[HTTP_MAX_DATA_LENGTH];
 unsigned char dataLength;
} HttpMessage;

typedef struct{
 const TcpConnection *connection;
 HttpMessage *message;
 unsigned char method[HTTP_MAX_METHOD_LENGTH+1];
 unsigned char url[HTTP_MAX_URL_LENGTH];
 unsigned char urlLength;
} HttpRequest;

typedef struct{
 unsigned short code;
 const char *message;
} HttpStatus;

typedef struct{
 unsigned short length;
 unsigned char *value;
} HttpHeaderValue;

#if HTTP_TCP_INCLUDED == 1
unsigned char TcpOnNewConnection(const unsigned char connectionId);
void TcpOnConnect(const unsigned char connectionId);
void TcpOnIncomingData(const unsigned char connectionId, const unsigned char *data, unsigned short dataLength);
void TcpOnDisconnect(const unsigned char connectionId);
#endif
const HttpHeaderValue HttpParseHeaderValue(const HttpMessage *message, const unsigned char *header);
void HttpOnIncomingRequest(const HttpRequest *request);

static unsigned char incomingRequestConnectionId;
static unsigned char incomingRequestState;
static HttpMessage incomingMessage;
static HttpRequest incomingRequest;

//*****************************************************************************************
//
// Function : HttpHeadersPutChar
// Description : put chat into http message structure why parsing headers string
//
//*****************************************************************************************
static unsigned char HttpHeadersPutChar(unsigned char ch){
 if((ch == '\n' || ch  == '\r') && incomingMessage.headersLenght == 0){
  return 1;
 }
 if(incomingMessage.headersLenght >= HTTP_MAX_HEADER_ROWS_LENGTH){
  return 0;
 }
 incomingMessage.headers[incomingMessage.headersLenght] = ch;
 incomingMessage.headersLenght++;
 return 1;
}

//*****************************************************************************************
//
// Function : HttpMessagePutData
// Description : function copy chars from data pointer into global HttpMessage object
//
//*****************************************************************************************
static void HttpMessagePutData(const unsigned char *data, unsigned short dataLength, const unsigned short dataPosition){
 if(dataLength == 0){
  return;
 }
 dataLength -= dataPosition;
 data += dataPosition;
 memcpy(incomingMessage.data + incomingMessage.dataLength, data, dataLength);
 incomingMessage.dataLength += dataLength;
}

//*****************************************************************************************
//
// Function : HttpParseHeader
// Description : function take char from incoming data and detect end of http header
// Return 1 on success 0 on header to long
//
//*****************************************************************************************
static unsigned char HttpParseHeader(const unsigned char ch, unsigned char *headerState){
 // parse headers end
 if(*headerState == HTTP_STATE_MAC_END_HEADER){
  if(ch == '\n'){
   *headerState = HTTP_STATE_WIN_END_HEADER;
   return HttpHeadersPutChar(ch);
  }
  if(ch == '\r'){
   *headerState = HTTP_STATE_END_HEADER;
   incomingMessage.dataLength = 0;
   incomingMessage.headers[incomingMessage.headersLenght] = 0;
   return 1;
  }
  *headerState = HTTP_STATE_HEADER;
 }
 if(*headerState == HTTP_STATE_LINUX_END_HEADER || *headerState == HTTP_STATE_WIN_END_HEADER2){
  if(ch == '\n'){
   *headerState = HTTP_STATE_END_HEADER;
   incomingMessage.dataLength = 0;
   incomingMessage.headers[incomingMessage.headersLenght] = 0;
   return 1;
  }
  *headerState = HTTP_STATE_HEADER;
 }
 if(*headerState == HTTP_STATE_WIN_END_HEADER){
  if(ch == '\r'){
   *headerState = HTTP_STATE_WIN_END_HEADER2;
   return 1;
  }
  *headerState = HTTP_STATE_HEADER;
 }
 // parse header rows
 if(*headerState == HTTP_STATE_HEADER){
  if(ch == '\r'){
   *headerState = HTTP_STATE_MAC_END_HEADER;
  }
  if(ch == '\n'){
   *headerState = HTTP_STATE_LINUX_END_HEADER;
  }
  return HttpHeadersPutChar(ch);
 }
 return 0;
}

//*****************************************************************************************
//
// Function : HttpSendResponseHeader
// Description : build http response header and send it into tcp connection
//
//*****************************************************************************************
static unsigned char HttpSendResponseHeader(const unsigned char connectionId, const HttpStatus *status, unsigned char *headers, unsigned short headersLength, unsigned short dataLength){
 if(incomingRequestState == HTTP_REQUEST_STATE_NO_REQUEST){
  return 0;
 }
 incomingRequestState = HTTP_REQUEST_STATE_START_REQUEST;
 {
  const unsigned char *statusMessage = status->message ?: "Shit Happens";
  // check length for first header row
  unsigned short length = 8 + 2 + 5 + strlen(statusMessage) + strlen(HTTP_HEADER_ROW_BREAK);
  if(length > HTTP_MAX_URL_LENGTH){
   TcpDisconnect(connectionId, 5000);
   return 0;
  }
  // check length for last header row
  length = 17 + 16 + 5 + (3 * strlen(HTTP_HEADER_ROW_BREAK));
  if(length > HTTP_MAX_URL_LENGTH){
   TcpDisconnect(connectionId, 5000);
   return 0;
  }
  snprintf(incomingRequest.url, HTTP_MAX_URL_LENGTH, "HTTP/1.0 %u %s" HTTP_HEADER_ROW_BREAK, status->code, statusMessage);
 }
 if(!TcpSendData(connectionId, 60000, incomingRequest.url, strlen(incomingRequest.url))){
  TcpDisconnect(connectionId, 5000);
  return 0;
 }
 if(headersLength){
  if(!TcpSendData(connectionId, 60000, headers, headersLength)){
   TcpDisconnect(connectionId, 5000);
   return 0;
  }
 }
 snprintf(incomingRequest.url, HTTP_MAX_URL_LENGTH, "Connection: close" HTTP_HEADER_ROW_BREAK "Content-Length: %u" HTTP_HEADER_ROW_BREAK HTTP_HEADER_ROW_BREAK, dataLength);
 if(!TcpSendData(connectionId, 60000, incomingRequest.url, strlen(incomingRequest.url))){
  TcpDisconnect(connectionId, 5000);
  return 0;
 }
 if(dataLength == 0){
  TcpDisconnect(connectionId, 5000);
 }
 return 1;
}

//*****************************************************************************************
//
// Function : HttpParseRequestHeader
// Description : function take char from incoming data and decide where put them in request structure on fail send error response header
// return 1 on success 0 on any error
//
//*****************************************************************************************
static unsigned char HttpParseRequestHeader(const unsigned char ch){
 if(incomingRequestState == HTTP_REQUEST_STATE_START_REQUEST){
  incomingRequestState = HTTP_REQUEST_STATE_METHOD;
  incomingRequest.urlLength = 0;
 }
 // parse http method
 if(incomingRequestState == HTTP_REQUEST_STATE_METHOD){
  if(ch == ' '){
   incomingRequestState = HTTP_REQUEST_STATE_URL;
   incomingRequest.method[incomingRequest.urlLength] = 0;
   incomingRequest.urlLength = 0;
   return 1;
  }
  if(ch < 'A' || ch > 'Z'){
   HttpStatus status = {400, "Header Syntax Error"};
   HttpSendResponseHeader(incomingRequestConnectionId, &status, 0, 0, 0);
   return 0;
  }
  if(incomingRequest.urlLength >= HTTP_MAX_METHOD_LENGTH){
   HttpStatus status = {431, "Header Part Too Long"};
   HttpSendResponseHeader(incomingRequestConnectionId, &status, 0, 0, 0);
   return 0;
  }
  incomingRequest.method[incomingRequest.urlLength] = ch;
  incomingRequest.urlLength++;
  return 1;
 }
 // parse url
 if(incomingRequestState == HTTP_REQUEST_STATE_URL){
  if(ch == ' '){
   incomingRequestState = HTTP_REQUEST_STATE_VERSION;
   return 1;
  }
  if(incomingRequest.urlLength >= HTTP_MAX_URL_LENGTH){
   HttpStatus status = {414, "URI Too Long"};
   HttpSendResponseHeader(incomingRequestConnectionId, &status, 0, 0, 0);
   return 0;
  }
  incomingRequest.url[incomingRequest.urlLength] = ch;
  incomingRequest.urlLength++;
  return 1;
 }
 // parse http version
 if(incomingRequestState == HTTP_REQUEST_STATE_VERSION){
  if(ch == '\r' || ch == '\n'){
   incomingMessage.headersLenght = 0;
   if(ch == '\r'){
    incomingRequestState = HTTP_STATE_MAC_END_HEADER;
   }
   if(ch == '\n'){
    incomingRequestState = HTTP_STATE_LINUX_END_HEADER;
   }
   return 1;
  }
  return 1;
 }
 // parse rest header rows
 if(incomingRequestState >= HTTP_STATE_LINUX_END_HEADER && incomingRequestState <= HTTP_STATE_HEADER){
  if(!HttpParseHeader(ch, &incomingRequestState)){
   HttpStatus status = {431, "Header Part Too Long"};
   HttpSendResponseHeader(incomingRequestConnectionId, &status, 0, 0, 0);
   return 0;
  }
  return 1;
 }
 HttpStatus status = {400, "Header Syntax Error"};
 HttpSendResponseHeader(incomingRequestConnectionId, &status, 0, 0, 0);
 return 0;
}

//*****************************************************************************************
//
// Function : HttpInit
// Description : function set memory into init state
//
//*****************************************************************************************
void HttpInit(){
 incomingRequestState = HTTP_REQUEST_STATE_NO_REQUEST;
 incomingRequestConnectionId = TCP_INVALID_CONNECTION_ID;
 incomingRequest.message = &incomingMessage;
}

//*****************************************************************************************
//
// Function : HttpParseHeaderValue
// Description : function parse header value from message
// return HttpHeaderValue structure with 0 pointer value property if header not found or header row have wrong syntax
//
//*****************************************************************************************
const HttpHeaderValue HttpParseHeaderValue(const HttpMessage *message, const unsigned char *header){
 HttpHeaderValue headerValue;
 headerValue.value = strstr(message->headers, header);
 if(!headerValue.value){
  headerValue.length = 0;
  return headerValue;
 }
 headerValue.value += strlen(header);
 headerValue.length = message->headersLenght - (headerValue.value - message->headers);
 if(!headerValue.length){
  headerValue.value = 0;
  return headerValue;
 }
 if(headerValue.value[0] != ':'){
  headerValue.value = 0;
  headerValue.length = 0;
  return headerValue;
 }
 if(headerValue.length >= 2 && headerValue.value[1] == ' '){
  headerValue.value += 2;
  headerValue.length -= 2;
 }else{
  headerValue.value++;
  headerValue.length--;
 }
 unsigned short i;
 for(i=0; i<headerValue.length; i++){
  if(headerValue.value[i] == '\n' || headerValue.value[i] == '\r'){
   headerValue.length = i;
   break;
  }
 }
 return headerValue;
}

//*****************************************************************************************
//
// Function : HttpSendResponse
// Description : function send response for client, can be used only once per request handling
//
//*****************************************************************************************
unsigned char HttpSendResponse(const HttpStatus *status, unsigned char *headers, unsigned short headersLength, unsigned char *data, unsigned short dataLength){
 if(incomingRequestState != HTTP_REQUEST_STATE_REQUEST_HANDLING){
  return 0;
 }
 unsigned char connectionId = incomingRequestConnectionId;
 if(!HttpSendResponseHeader(connectionId, status, headers, headersLength, dataLength)){
  return 0;
 }
 if(dataLength == 0){
  return 1;
 }
 if(!TcpSendData(connectionId, 60000, data, dataLength)){
  TcpDisconnect(connectionId, 5000);
  return 0;
 }
 TcpDisconnect(connectionId, 5000);
 return 1;
}

//*****************************************************************************************
//
// Function : HttpTcpOnNewConnection
// Description : defined tcp callback for accept new incoming connection on http port
//
//*****************************************************************************************
unsigned char HttpTcpOnNewConnection(const unsigned char connectionId){
 const TcpConnection *connection = TcpGetConnection(connectionId);
 if(connection->port != HTTP_SERVER_PORT){
  #if HTTP_TCP_INCLUDED == 1
  return TcpOnNewConnection(connectionId);
  #else
  return NET_HANDLE_RESULT_DROP;
  #endif
 }
 if(incomingRequestState != HTTP_REQUEST_STATE_NO_REQUEST){
  return NET_HANDLE_RESULT_DROP;
 }
 incomingRequestConnectionId = connectionId;
 incomingRequestState = HTTP_REQUEST_STATE_START_REQUEST;
 incomingRequest.connection = connection;
 return NET_HANDLE_RESULT_OK;
}

//*****************************************************************************************
//
// Function : HttpTcpOnConnect
// Description : defined tcp callback for detect new connection not used in http
//
//*****************************************************************************************
void HttpTcpOnConnect(const unsigned char connectionId){
 #if HTTP_TCP_INCLUDED == 1
 if(connectionId == incomingRequestConnectionId){
  return;
 }
 TcpOnConnect(connectionId);
 #endif
}

//*****************************************************************************************
//
// Function : HttpTcpOnIncomingData
// Description : handle incoming http request by tcp callback
//
//*****************************************************************************************
void HttpTcpOnIncomingData(const unsigned char connectionId, const unsigned char *data, unsigned short dataLength){
 if(connectionId != incomingRequestConnectionId){
  #if HTTP_TCP_INCLUDED == 1
  TcpOnIncomingData(connectionId, data, dataLength);
  #endif
  return;
 }
 if(incomingRequestState == HTTP_REQUEST_STATE_NO_REQUEST || connectionId != incomingRequestConnectionId){
  TcpDisconnect(connectionId, 5000);
  return;
 }
 // parse request headers
 unsigned short dataPosition = 0;
 if(incomingRequestState < HTTP_STATE_END_HEADER){
  for(dataPosition=0; dataPosition<dataLength; dataPosition++){
   if(!HttpParseRequestHeader(data[dataPosition])){
    return;
   }
   if(incomingRequestState == HTTP_STATE_END_HEADER){
    dataPosition++;
    break;
   }
  }
  if(incomingRequestState != HTTP_STATE_END_HEADER){
   return;
  }
 }
 // read request body
 if(incomingRequestState < HTTP_STATE_END_MESSAGE){
  if(strcmp(incomingRequest.method, "HEAD") == 0 || strcmp(incomingRequest.method, "GET") == 0 || strcmp(incomingRequest.method, "OPTIONS") == 0){
   incomingRequestState = HTTP_STATE_END_MESSAGE;
  }else{
   const HttpHeaderValue contentLength = HttpParseHeaderValue(&incomingMessage, "Content-Length");
   if(!contentLength.value){
    HttpStatus status = {411, "Length Required"};
    HttpSendResponseHeader(connectionId, &status, 0, 0, 0);
    return;
   }
   unsigned long httpDataLength;
   if(!ParseLong(&httpDataLength, contentLength.value, contentLength.length)){
    HttpStatus status = {400, "Header Syntax Error"};
    HttpSendResponseHeader(connectionId, &status, 0, 0, 0);
    return;
   }
   if(httpDataLength > HTTP_MAX_DATA_LENGTH){
    HttpStatus status = {413, "Payload Too Large"};
    HttpSendResponseHeader(connectionId, &status, 0, 0, 0);
    return;
   }
   HttpMessagePutData(data, dataLength, dataPosition);
   if(incomingMessage.dataLength == httpDataLength){
    incomingRequestState = HTTP_STATE_END_MESSAGE;
   }else{
    return;
   }
  }
 }
 // handle request and send response
 if(incomingRequestState == HTTP_STATE_END_MESSAGE){
  incomingRequestState = HTTP_REQUEST_STATE_REQUEST_HANDLING;
  HttpOnIncomingRequest(&incomingRequest);
  if(incomingRequestState == HTTP_REQUEST_STATE_REQUEST_HANDLING){
   TcpDisconnect(connectionId, 5000);
   return;
  }
  return;
 }
 HttpStatus status = {400, "Request Is In Invalid State"};
 HttpSendResponseHeader(connectionId, &status, 0, 0, 0);
 return;
}

//*****************************************************************************************
//
// Function : HttpTcpOnDisconnect
// Description : after handling request reset memory and be ready for another http request
//
//*****************************************************************************************
void HttpTcpOnDisconnect(const unsigned char connectionId){
 if(connectionId != incomingRequestConnectionId){
  #if HTTP_TCP_INCLUDED == 1
  TcpOnDisconnect(connectionId);
  #endif
  return;
 }
 if(incomingRequestState == HTTP_REQUEST_STATE_NO_REQUEST){
  return;
 }
 incomingRequestConnectionId = TCP_INVALID_CONNECTION_ID;
 incomingRequestState = HTTP_REQUEST_STATE_NO_REQUEST;
}
#endif
