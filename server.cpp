#include<stdio.h>
#include<iostream>
#include<fstream>
#include<algorithm>
#include<string.h>
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<pthread.h>
#include<list>
#include<time.h>
#include"json.h"

using namespace std;

/*------------------------------PROTO------------------------------*/
std::list<struct notification>::iterator search_notif(list<struct notification> &lista,string clientID);
std::list<struct activeClient>::iterator search_client(list<struct activeClient> &lista,string clientID);
const std::string currentDateTime();
void *client_function(void *);
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

/*-------------------------GLOBAL VARIABLES------------------------*/
struct notification
{
	string text;
	string sender;
	string id;
	string app;
};

struct activeClient
{
	string id;
	string nfc;
};

list<struct notification> notification_list; //pending notifications
list<struct activeClient> activeClient_list; //active clients
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

/*-----------------------------MUTEX-------------------------------*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/
int main(int argc , char *argv[])
{

    int socket_desc , client_sock , c;

    struct sockaddr_in server , client;


    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("Could not create socket");
    }
    puts("Socket created!");


    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons( 8889 );


    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {

        perror("bind failed. Error");
        return 1;
    }
    puts("Bind done!");

    //Listen
    listen(socket_desc , 3);

    puts("Waiting for incoming connections...\n");

    c = sizeof(struct sockaddr_in);
    pthread_t thread_fork;

    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Connection accepted!");

        if( pthread_create( &thread_fork , NULL ,  client_function , (void*) &client_sock) < 0)
        {
            perror("could not create thread");
            return 1;
        }

        pthread_detach(thread_fork);
        puts("Handler assigned");
    }

    if (client_sock < 0)
    {
        perror("accept failed");
        return 1;
    }

    return 0;
}
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/


/*-----------------------------CLIENT THREAD-------------------------------*/
void *client_function(void *socket_desc)
{
    int sock = *(int*)socket_desc;
    int read_size, i;
    char client_message[2000], client_sms_aux[2000];
    string output, output_final;

    Json::Value parsedFromString;
    Json::Value array, clientID, nfc;
    Json::Reader reader;
    notification notif_smartphone;
    activeClient clientON;

    while((read_size=recv(sock, client_message,sizeof(client_message),0))>0)
    {

	for(i=0;;i++)
	{
		client_sms_aux[i]=client_message[i];
		if(client_message[i]=='\n')
			break;
	}
	client_sms_aux[i+1]='\0';
	strcpy(client_message,client_sms_aux);

	printf("%s\n",client_message);

	if (!reader.parse(client_message, parsedFromString))
	{
		cout << "ERROR json" << endl;
	}
		array = parsedFromString["state"];

	/*-----------------------------MACHINE STATION-------------------------------*/
	if(!array.compare("login"))	//receive login from machine station and send training file for that session
	{
		pthread_mutex_lock (&mutex); //lock mutex

		clientID = parsedFromString["code"];

		std::list<struct activeClient>::iterator it;

        it=search_client(activeClient_list,clientID.asString());

		if(it!=activeClient_list.end())
		{
			Json::StreamWriterBuilder builder;

			Json::Value state;
			state["state"]="ok_login";

			string loginState=Json::writeString(builder, state);
			loginState.erase(std::remove(loginState.begin(), loginState.end(), '\n'), loginState.end());
			loginState+="\n";

			cout << loginState << endl;

			write(sock,loginState.c_str(),loginState.size());
		}
		else
		{
			Json::StreamWriterBuilder builder;

			Json::Value state;
			state["state"]="error_login";

			string loginState=Json::writeString(builder, state);
			loginState.erase(std::remove(loginState.begin(), loginState.end(), '\n'), loginState.end());
			loginState+="\n";

			send(sock,loginState.c_str(),loginState.size(),0);
		}
		pthread_mutex_unlock (&mutex); //unlock mutex
	}
	else if(!array.compare("plan"))
	{
		pthread_mutex_lock (&mutex); //lock mutex

		string currDate=currentDateTime();
		currDate=currDate.substr (0,10);
		currDate+=".txt";
		ifstream ifs (currDate.c_str());//open file

		if (ifs.is_open())
		{
			while (!ifs.eof())
			{
				ifs >> output;
				output_final += output;
			}
			cout << endl << output_final << endl;

			output_final+="\n";

			write(sock,output_final.c_str(),output_final.size()); //send back the training plan

			output_final.clear();//clear string with training plan
		}
		else
		{
			Json::StreamWriterBuilder builder;

			Json::Value state;
			state["state"]="error_plan";

            string planState=Json::writeString(builder, state);
			planState.erase(std::remove(planState.begin(), planState.end(), '\n'), planState.end());
			planState+="\n";

			send(sock,planState.c_str(),planState.size(),0);

		}
		ifs.close(); //close file

		pthread_mutex_unlock (&mutex); //unlock mutex
	}
	else if(!array.compare("logout"))	//receive logout from machine station and save updated training file
	{
		while((read_size=recv(sock, client_message,sizeof(client_message),0))>0)
		{
			for(i=0;;i++)
			{
				client_sms_aux[i]=client_message[i];
				if(client_message[i]=='\n')
					break;
			}
			client_sms_aux[i+1]='\0';
			strcpy(client_message,client_sms_aux);

			printf("%s\n",client_message);
		}

		ofstream file;
		string currDate=currentDateTime();
		currDate=currDate.substr (0,10);
		currDate+=".txt";
		file.open(currDate.c_str());
		if(!file.is_open()) cout << "ERROR OPEN FILE" << endl;
		file << client_message;
		file.close();

	}
	else if(!array.compare("historic")) //get the historic from date given
	{
		array = parsedFromString["date"];

		pthread_mutex_lock (&mutex); //lock mutex

		string sdate=array.asString();
		sdate+=".txt";

		ifstream ifs (sdate.c_str());//open file

		if (ifs.is_open())
		{
			while (!ifs.eof())
			{
				ifs >> output;
				output_final += output;
			}
			cout << endl << output_final << endl;

			output_final+="\n";

			send(sock,output_final.c_str(),output_final.size(),0); //send back the training plan

			output_final.clear();//clear string with training plan
		}
		else //return error if file doesn't exist
		{
			Json::StreamWriterBuilder builder;

			Json::Value error;
			error["state"]="error_historic";

			string historic=Json::writeString(builder, error);
			historic.erase(std::remove(historic.begin(), historic.end(), '\n'), historic.end());
			historic+="\n";

			send(sock,historic.c_str(),historic.size(),0);
		}

		ifs.close(); //close file

		pthread_mutex_unlock (&mutex); //unlock mutex
	}
	/*---------------------------------------------------------------------------*/
	/*---------------------------------------------------------------------------*/

	/*--------------------------------SMARTPHONE---------------------------------*/
	else if(!array.compare("PHONE_LOGIN"))	//receive phone login and put client active
	{
		pthread_mutex_lock (&mutex); //lock mutex
		ifstream ifs ("clients.txt");//open file
		clientID = parsedFromString["id"];
		nfc = parsedFromString["nfctext"];

		Json::Reader readClients;
        Json::Value rClients;

		if (ifs.is_open())
		{
			while (!ifs.eof())
			{
				ifs >> output;
				output_final += output;
			}
			cout << endl << output_final << endl;

            readClients.parse(output_final,rClients);

            if(rClients.isMember("id")==true)
            {
                Json::Value rID;

                rID = rClients["id"];

                if(!clientID.compare(rID))
                {
                    clientON.nfc=nfc.asString();
                    clientON.id=clientID.asString();

                    activeClient_list.push_front(clientON);
                }
            }
        }
        else
            cout << "ERROR" << endl;

		ifs.close(); //close file

		output_final.clear();//clear string with training plan

		pthread_mutex_unlock (&mutex); //unlock mutex
	}
	else if(!array.compare("PHONE_LOGOUT"))	//receive phone logout and update active clients list
	{
		clientID = parsedFromString["id"];

        string client = clientID.asString();

		std::list<struct activeClient>::iterator findListClient;
		std::list<struct notification>::iterator findListNotif;

		while(1)
		{
            findListClient=search_client(activeClient_list,client);
            if(activeClient_list.empty() || (findListClient==activeClient_list.end()))
                break;
            else
                activeClient_list.erase(findListClient);
		}

		while(1)
		{
            findListNotif=search_notif(notification_list,client);
            if(notification_list.empty() || (findListNotif==notification_list.end()))
                break;
            else
                notification_list.erase(findListNotif);
		}
	}
	else if(!array.compare("NOTIFICATION"))	//receive notification from smartphone
	{
		Json::Value client_id = parsedFromString["client_id"];
		Json::Value text = parsedFromString["context"];
		Json::Value sender = parsedFromString["sender"];
		Json::Value app = parsedFromString["app"];
		notif_smartphone.id=client_id.asString();
		notif_smartphone.text=text.asString();
		notif_smartphone.sender=sender.asString();
		notif_smartphone.app=app.asString();

		notification_list.push_front(notif_smartphone);
	}
	/*---------------------------------------------------------------------------*/
	/*---------------------------------------------------------------------------*/

	if(!notification_list.empty())	//check if exist any pending notifications
	{
		Json::StreamWriterBuilder builder;

		std::list<struct notification>::iterator findList = search_notif(notification_list,clientID.asString());

		if(findList!=notification_list.end())
		{
			Json::Value value;
			value["state"]="notification";
			value["app"]=notif_smartphone.app;
			value["person"]=notif_smartphone.sender;
			value["text"]=notif_smartphone.text;

			string notification=Json::writeString(builder, value);
            notification.erase(std::remove(notification.begin(), notification.end(), '\n'), notification.end());

			notification+="\n";

			send(sock,notification.c_str(),notification.size(),0);

			notification_list.erase(findList);
		}
	}
	memset(client_message,sizeof(client_message),0);
    }

    if(read_size == 0)
    {
        puts("Client disconnected");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("recv failed");
    }

    return 0;
}

/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

/*--------------------------------SEARCH FUNCTIONS---------------------------------*/
std::list<struct notification>::iterator search_notif(list<struct notification> &lista,string clientID)
{
	std::list<struct notification>::iterator it;
	for(it=lista.begin();it!=lista.end();it++)
	{
		if(!clientID.compare((*it).id))
			break;
	}

	if(!(it == lista.end())) //end of the list
		return it;
	else
		return lista.end();
}

std::list<struct activeClient>::iterator search_client(list<struct activeClient> &lista,string clientID)
{
	std::list<struct activeClient>::iterator it;
	for(it=lista.begin();it!=lista.end();it++)
	{
		if(!clientID.compare((*it).id))
			break;
	}

	if(!(it == lista.end())) //end of the list
		return it;
    else
		return lista.end();
}
/*-----------------------------------------------------------------*/
/*-----------------------------------------------------------------*/

/*--------------------------------GET CURRENT DATA FUNCTION---------------------------------*/
const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);

    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}
