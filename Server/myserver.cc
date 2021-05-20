//
//  myserver.cc
//  PA2
//
//  Created by Saad on 10/26/10.
//  Copyright © 2010 Saad. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stddef.h>
#include <stdarg.h>

#include <iostream>
#include <string>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>

#include <unistd.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <vector>
#include <fstream>
#include <sstream>


//to directly use cout and other methods instead of std::cout
using namespace std;

//constants
static const int BUFLEN = 1024;
static const string COMMANDS_LIST[] = { "LIST", "CREATE", "SEND", "RECEIVE", "DELETE", "HELP", "EXIT"};
static const string COMMANDS_PARAMS[] = { " <SERVER|CLIENT>", " <SERVER|CLIENT> <FILENAME>", " <filename>", " <filename>", " <SERVER|CLIENT> <FILENAME>", "", ""};
static const int MAX_COMMANDS = 7, LIST_CMD = 0, CREATE_CMD = 1 ,SEND_CMD = 2, RECV_CMD = 3, DELTE_CMD = 4,  HELP_CMD = 5, EXIT_CMD = 6;
static const string TEMP_FILE = ".temp_data"; // file for temporarily storing the data to send
static const string ERROR_CMD = "Command Error\n";
static const string EOF_FOUND = "..400EOF..";

//prototypes
void startServerAndListen(int serverPort);


int main(int argc, char* argv[])
{
    if(argc < 2 || atoi(argv[1]) < 1024)
    {
        perror("Usage: myserver <port number between 1024 and 65535>");
        exit(-1);
    }
    int portNumber =  atoi(argv[1]);

    cout <<"Starting server at port "<<portNumber<<endl;

    startServerAndListen(portNumber); // run the server

    return -1;
}
std::string trim(const std::string &s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && isspace(*it))
        it++;

    std::string::const_reverse_iterator rit = s.rbegin();
    while (rit.base() != it && isspace(*rit))
        rit++;

    return std::string(it, rit.base());
}
vector<string> split(string input, const string& delimitor)
{
    vector<string> result_vec;
    
    char* str = new char[input.length()];
    strcpy(str,input.c_str());
    char *result = NULL;
    result = strtok( str, delimitor.c_str() );
    while( result != NULL )
    {
        string s = result;
        result_vec.push_back(s); // push the string into the vector
        
        result = strtok( NULL , delimitor.c_str() );
    }
    delete []str; // delete the char array
    return result_vec;
}
void sendMessage(int sockfd, string msg)
{
    if (send(sockfd, (void*)msg.c_str(), strlen(msg.c_str()), 0) < 0)
    { // error while sending

        perror("Message sending faild.");

        return; // exit the function
    }

}
void sendMessage(int sockfd, string msg, long msg_len)
{
    long bytes_sent = 0, pbuf = 0;
    char* msg_chars = new char[msg_len];
    strcpy(msg_chars, msg.c_str());
    while (pbuf < msg_len)
    {
        if ((bytes_sent = send(sockfd, (void*)(msg_chars + pbuf), msg_len, 0)) < 0)
        {
            perror("Error while sending");
            return; // exit the function
        }
        pbuf += bytes_sent; // update pointer
    }
    delete []msg_chars; // free memory
}

int stricmp(char const *a, char const *b)
{
    for (;; a++, b++) {
        int d = tolower(*a) - tolower(*b);
        if (d != 0 || !*a)
            return d;
    }
    return 0;//successfull compare
}
int commandIndex(string command)
{
    for (int i = 0; i < MAX_COMMANDS; ++i)
    {
        if (stricmp(command.c_str(), COMMANDS_LIST[i].c_str()) == 0)//case insenstive compare
            return i; // success!
    }
    return -1;
}

long getFileSize(string filename)
{
    // open the file
    int fd = open(filename.c_str(), O_RDONLY);
    
    if (fd == -1)
        return -1; // the file doesn't exist!
    
    long filesize = lseek(fd, 0, SEEK_END);
    
    close(fd); // close the file
    
    return filesize;
}
void sendTempCreatedFile(int clientfd, string filename, long filesize)
{
    char output[BUFLEN]; // the array of chars that'll contain the output chars read from the file
    long bytes_read = 0;
    long total_bytes_read = 0;
    
    cout << "sending file ... " << endl;
    
    // open the file for reading
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd == -1)
    {
        perror("errpr file opening");
        return;
    }
    
    while (total_bytes_read < filesize)
    { // run this loop till total num of bytes read equals filesize
        bytes_read = read(fd, output, BUFLEN-2); // read data leaving buffer space for a \0 and a \n
        total_bytes_read += bytes_read;
    }
    
    cout << "total bytes read: " << total_bytes_read << endl;
    output[bytes_read-1] = '\n'; // string terminate the file content and add a newline too
    output[bytes_read] = '\0';
    
    sendMessage(clientfd, output, bytes_read+1);
    
    close(fd); // close the open file
    
}
void cmdReceive(int sockfd_clt, string inputCommand)
{
    
    vector<string> splitted_input = split(inputCommand.c_str(), " ");
    
    string response;
    if (splitted_input.size() < 2)
    {
        cout<<"Invalid RECEIVE command by client. Sending usage to client.";
        response = "Invalid Command! Usage 'RECEIVE': RECEIVE <filename>\n";
        sendMessage(sockfd_clt, response);

    }
    else
    {
   
        string filename = splitted_input[1];
    
        cout<<"received request to send file "<<filename<<endl;
        char buffer[BUFLEN];
        
        
        if(!ifstream(filename.c_str()))
        {
            //file doesn't exists
            response = "File " + filename + " doesn not exist";
            sendMessage(sockfd_clt, response);
            return;//return
        }
        sendMessage(sockfd_clt, "200 Ok. File found, started sending file.");
        long fileSize = getFileSize(filename.c_str()), sentBytes = 0;
        FILE *file = fopen(filename.c_str(), "rb");
        if (!file)
        {
            printf("Can't open file for reading\n");
            sendMessage(sockfd_clt, "File Corrupted at server.");

            return;
        }
        
        while (!feof(file))
        {
            long rval = fread(buffer, 1, sizeof(buffer), file);
            if (rval < 1 && sentBytes < fileSize - 1)
            {
                printf("Can't read from file\n");
                sendMessage(sockfd_clt, "File Corrupted at server.");
                fclose(file);
                return;
            }
            
            int off = 0;
            do
            {
                long sent = send(sockfd_clt, &buffer[off], rval - off, 0);
                sentBytes += sent;
                cout<<"\n sent "<<sentBytes<<" of "<<fileSize;
                if(fileSize % BUFLEN == 0 && fileSize == sentBytes)
                {
                    //file has all chunks of 8192 and receiver need to be notifed as the check at receiver end is of 0 or less than BUFLEN. So it will wait until either 0 or less bytes than BUFLEN Sent.
                    send(sockfd_clt, &EOF_FOUND, sizeof(EOF_FOUND), 0);//just snd an empty
                }
                
                if (sent < 1 && sentBytes < fileSize - 1)
                {
                    // if the socket is non-blocking, then check
                    // the socket error for WSAEWOULDBLOCK/EAGAIN
                    // (depending on platform) and if true then
                    // use select() to wait for a small period of
                    // time to see if the socket becomes writable
                    // again before failing the transfer...
                    
                    printf("Can't write to socket");
                    fclose(file);
                    return;
                }
                
                off += sent;
            }
            while (off < rval);
        }
        cout<<"File base been sent";
        fclose(file);
    }
}
int write_all(FILE *file, const char *buf, long len)
{
    while (len > 0)
    {
        long written = fwrite(buf, 1, len, file);
        if (written < 1)
            return -1;
        
        buf += written;
        len -= written;
    }
    
    return 0;
}

void cmdSEND(int sockfd_clt, string command)
{
    cout<<"Received Command"<<command<<endl;
    vector<string> splitted_input = split(command.c_str(), " ");
    
    string response;
    if (splitted_input.size() != 3)
    {
        cout<<"\nInvalid SEND command by client. Sending usage to client.\n";
        response = "Invalid Command! Usage 'SEND': SEND <filename> <fileSize>";
        sendMessage(sockfd_clt, response);
        
    }
    else
    {
        if(ifstream(splitted_input[1].c_str()))
        {
            //file already exisits. Rename File
            cout<<"\nFile '"<<splitted_input[1]<<"' already exists.\n";
            time_t now = time(0);
            std::stringstream ss;
            ss << now;
            string ts = ss.str();
            splitted_input[1] = ts.append("_").append(splitted_input[1]);
        }
        long long fileSize = atoll(splitted_input[2].c_str());
        cout<<"FileSize: "<<fileSize<<endl;
        if (fileSize <= 0) {
            cout<<"Wrong file size"<<endl;
            return;
        }
        sendMessage(sockfd_clt, "200-FILEHANDSHAKE OK. Server is ready to receive the file");//send status that file found.
        
        //found file.
        cout<<"\nReceiving file... \n";
        long rval;
        char buf[BUFLEN];
        FILE *file = fopen(splitted_input[1].c_str(), "wb");
        if (!file)
        {
            cout<<"Can't open file for writing at receiving side.\n";
            return;
        }
        long long totalBytesWritten = 0;
        do
        {
            rval = recv(sockfd_clt, buf, sizeof(buf), 0);
            trim(buf);
            if (strlen(buf)==0) {
                continue;
            }
            if (strcmp(buf,EOF_FOUND.c_str()) == 0) {//this is a special case where receiver is waiting for more bytes but sender ended as it's last chunk was equal to 8192 bytes
                break;
            }
            if (strcmp(buf,"EOF_FOUND_FILE_COMPLETED") == 0)
            {
                cout<<"Found EOF_FOUND_FILE_COMPLETED";
                break;
            }
            cout<<"Received chunk of "<<rval<<" bytes. Wrriten "<<totalBytesWritten+rval<<" out of "<<fileSize<<endl;
            if (rval < 0)
            {
                // if the socket is non-blocking, then check
                // the socket error for WSAEWOULDBLOCK/EAGAIN
                // (depending on platform) and if true then
                // use select() to wait for a small period of
                // time to see if the socket becomes readable
                // again before failing the transfer...
                
                printf("Can't read from socket\n");
                return;
            }
            
//            if (rval == 0 || rval < sizeof(buf))
//                break;
            
            if (write_all(file, buf, rval) == -1)
            {
                printf("Can't write to file\n");
                return;
            }
            totalBytesWritten += rval;

        }while (true);
//        }while (totalBytesWritten<fileSize);//actual check. set to disabled when bufferrs are not properly cleared

        cout<<"File received and created file with name '"<<splitted_input[1]<<"'"<<endl;
        fclose(file);
        response = "200-FILERECEIVE OK. File '"+ splitted_input[1] +"' received by server";
        sendMessage(sockfd_clt, response);//send success message of receive.
    }
}
void cmdDelete(int sockfd_clt, string inputCommand)
{
    vector<string> splitted_input = split(inputCommand.c_str(), " ");
    string response;
    if (splitted_input.size() < 3) {
        cout<<"Invalid DELETE command by client. Sending usage to client.";
        response = "Invalid Command! Usage 'DELETE': DELETE <server|client> <filename>\n";
    }
    else
    {
        string filename = splitted_input[2];
        if(!ifstream(filename.c_str()))
        {
            //file doesn't exists. create a new one.
            response = "File '" + filename + "' does not exist on server.";
        }
        else
        {
            remove(filename.c_str()); // delete file

            if (!ifstream(filename.c_str())) {
                //file not found
                response = "File '" + filename + "' has been deleted from server.";
            }
            else
            {
                //Failed to delete
                response = "File '" + filename + "' could not be deleted at server.";
            }
        }
    }
    response = response + "\n";
    sendMessage(sockfd_clt, response);
}
void cmdListAction(int sockfd_clt)
{
    string output; // the output
    
    output = "/bin/ls > " + TEMP_FILE;
    system(output.c_str());
    // get the file size
    long filesize = getFileSize(TEMP_FILE);
    
    // read the file
    if (filesize > -1)
    {
        sendTempCreatedFile(sockfd_clt, TEMP_FILE, filesize); // send the file to the client
    }
    else
    {
        sendMessage(sockfd_clt, ERROR_CMD + "\n"); // error occurred while listingfile
    }
    
    // remove the temporary file
    output = "rm -f " + TEMP_FILE;
    system(output.c_str());
}
void cmdCreateFile(int sockfd_clt, string inputCommand)
{
    vector<string> splitted_input = split(inputCommand.c_str(), " ");
    string response;
    if (splitted_input.size() < 3) {
        cout<<"Invalid Create command by client. Sending usage to client.";
        response = "Invalid Command! Usage 'CREATE': CREATE <server|client> <filename>\n";
    }
    else
    {
        string filename = splitted_input[2];
        if(!ifstream(filename.c_str()))
        {
            //file doesn't exists. create a new one.

            fstream stream;

            stream.open(filename.c_str(),  fstream::in | fstream::out | fstream::trunc);
            stream<<"File Created by 'Create Server Command";
            cout<<"Created file with name: "<<filename;
            stream.close();
            response = "Created a file at server with filename: " + filename;
        }
        else
        {
            
            cout<<"Couldn't create file. A file with name "<<filename<<" already exists";
            response = "Couldn't create file. A file with name " + filename + " already exists at server.";
        }
    }
    response = response + "\n";
    sendMessage(sockfd_clt, response);
}
void cmdHelp(int sockfd_clt)
{
    string response = "Following commands are available:\n";
    for (int i = 0; i < MAX_COMMANDS; i++)
    {
        response = response + "\t" + COMMANDS_LIST[i] + COMMANDS_PARAMS[i] + "\n";
    }
    response = response + "\n";
    sendMessage(sockfd_clt, response);
}

bool evaluateClientInput(string input, const struct sockaddr_in clt_addr, int sockfd_clt)
{
    vector<string> splitted_input = split(input.c_str(), " ");
    
    int cm_index = commandIndex(splitted_input.size() > 0 ? splitted_input[0] : "help");
    switch(cm_index)
    {
        case LIST_CMD:
            cmdListAction(sockfd_clt);
            break;
        case HELP_CMD:
            cmdHelp(sockfd_clt);
            break;
        case CREATE_CMD:
            cmdCreateFile(sockfd_clt, input);
            break;
        case RECV_CMD:
            cmdReceive(sockfd_clt, input);
            break;
        case SEND_CMD:
            cmdSEND(sockfd_clt, input);
            break;
        case DELTE_CMD:
            cmdDelete(sockfd_clt, input);
            break;
        case EXIT_CMD:
            cout<<"Connection closed by client.\n";
            close(sockfd_clt);// close client socket

            return false;
        default:
            cout<<"Invalid command by client. Sending Help list to client."<<endl;
//            cmdHelp(sockfd_clt);
    }
    
    return true; // signal the caller the user DOES NOT want to exit
    
}

void startServerAndListen(int serverPort)
{
    int sockfd_lis, sockfd_clt;

    struct sockaddr_in srv_adr, clt_adr;

    srv_adr.sin_family = AF_INET;
    srv_adr.sin_port = htons(serverPort);
    srv_adr.sin_addr.s_addr = INADDR_ANY;
    memset(&(srv_adr.sin_zero), '\0', 8);


    cout<<".. creating local listener socket\n";
    if ((sockfd_lis = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Could not create socket.");
        exit(1);
    }

    //socket binding
    cout<<".. binding socket to port: "<<serverPort<<endl;
    if (::bind(sockfd_lis, (const struct sockaddr *)&srv_adr, sizeof(struct sockaddr)) == -1)//:: used to  make it a use of global library as std has a same method and we have used using namespace std.
    {
        perror("Socking binding error. May be this socket already in use.");
                // close socket
        close(sockfd_lis);

        exit(1);
    }

    // listen at the socket

    cout<<".. starting to listen at the port\n";
    if (listen(sockfd_lis, 1) == -1) {
        perror("Could not started to listen.");
        // close socket
        close(sockfd_lis);

        exit(1);
    }

    // wait for connection
    cout<<".. Server Started...\n";
    
    socklen_t size = sizeof(struct sockaddr_in); // the size of the incoming sockaddr_in

WaitForAnotherClient:
    cout<<"waiting for some client to connect...\n";
    if ((sockfd_clt = accept(sockfd_lis, (struct sockaddr *)&clt_adr, &size)) == -1)
    { // error occured
        perror("Cannot accept the connection");
        // close server socket
        close(sockfd_lis);
        exit(1);
    }

    //connected.
    cout<<"\nClient Connected to Server through ip: "<<inet_ntoa(clt_adr.sin_addr)<<" at port: "<<ntohs(clt_adr.sin_port)<<"\n";

    //send welcome message to client
    sendMessage(sockfd_clt, "01-Hello from server...");

    char input[BUFLEN]; // get the commands
    do
    {
        long recv_num = recv(sockfd_clt, (void*)input, BUFLEN-1, 0);
        if(recv_num == 0)
        { // client closed the connection
            cout << "Client no longer connected. Connection was closed by client ..." << endl;
        }

        if(recv_num > 1)
            input[recv_num - 1] = '\0'; // Last charecter will be '\n'. Remove that
        else
            input[recv_num] = '\0';
        
        cout<<inet_ntoa(clt_adr.sin_addr)<<":/> "<<input<< endl;

    } while(evaluateClientInput(input, clt_adr, sockfd_clt));//until exit. keep receiving


    //will wait for a new connection
    goto WaitForAnotherClient;

    //commented this line as it will never called in this case
    //close(sockfd_lis);

}


