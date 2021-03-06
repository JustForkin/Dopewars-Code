/* serverside.c   Handles the server side of dopewars                   */
/* copyright (c)  1998-2000  ben webb                                   */
/*                Email: ben@bellatrix.pcl.ox.ac.uk                     */
/*                WWW: http://bellatrix.pcl.ox.ac.uk/~ben/dopewars/     */

/* This program is free software; you can redistribute it and/or        */
/* modify it under the terms of the GNU General Public License          */
/* as published by the Free Software Foundation; either version 2       */
/* of the License, or (at your option) any later version.               */

/* This program is distributed in the hope that it will be useful,      */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of       */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        */
/* GNU General Public License for more details.                         */

/* You should have received a copy of the GNU General Public License    */
/* along with this program; if not, write to the Free Software          */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston,               */
/*                   MA  02111-1307, USA.                               */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>
#include "dopeos.h"
#include "serverside.h"
#include "dopewars.h"
#include "message.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifndef SD_SEND
#define SD_SEND 1
#endif
#ifndef SD_RECV
#define SD_RECV 0
#endif

/* Maximum time, in seconds, between reports from this server to the   */
/* metaserver. Setting this to be too small will most likely annoy the */
/* metaserver maintainer, and result in your host being blocked... ;)  */
#define METATIME (1200)

int TerminateRequest,ReregisterRequest;

int MetaTimeout;
char WantQuit=FALSE;

GSList *FirstServer=NULL;

static GScanner *Scanner;

/* Pointer to the filename of a pid file (if non-NULL) */
char *PidFile;

static char HelpText[] = { 
 N_("dopewars server version %s commands and settings\n\n"
    "help                       Displays this help screen\n"
    "list                       Lists all players logged on\n"
    "push <player>              Politely asks the named player to leave\n"
    "kill <player>              Abruptly breaks the connection with the "
                               "named player\n"
    "msg:<mesg>                 Send message to all players\n"
    "quit                       Gracefully quit, after notifying all players\n"
    "<variable>=<value>         Sets the named variable to the given value\n"
    "<variable>                 Displays the value of the named variable\n"
    "<list>[x].<var>=<value>    Sets the named variable in the given list,\n"
    "                           index x, to the given value\n"
    "<list>[x].<var>            Displays the value of the named list variable\n"
    "\nValid variables are listed below:-\n\n")
};

int SendSingleHighScore(Player *Play,struct HISCORE *Score,
                        int index,char Bold);

int SendToMetaServer(char Up,int MetaSock,char *data,
                     struct sockaddr_in *MetaAddr) {
/* Sends server details, and any additional data, to the metaserver */
   GString *text;
   int numbytes;
   text=g_string_new("");
   g_string_sprintf(text,"R:%d\n%d\n%s\n%s",
                    METAVERSION,Port,MetaServer.LocalName,MetaServer.Password);
   if (data) { g_string_append(text,"\n"); g_string_append(text,data); }
   numbytes=sendto(MetaSock,text->str,strlen(text->str),0,
                   (struct sockaddr *)MetaAddr,sizeof(struct sockaddr));
   g_string_free(text,TRUE);
   if (numbytes==-1) {
      g_warning(_("cannot send data to metaserver\n"));
      return 0;
   }
   return 1;
}

void RegisterWithMetaServer(char Up,char SendData) {
/* Sends server details to the metaserver, if specified. If "Up" is  */
/* TRUE, informs the metaserver that the server is now accepting     */
/* connections - otherwise tells the metaserver that this server is  */
/* about to go down. If "SendData" is TRUE, then also sends game     */
/* data (e.g. scores) to the metaserver. If networking is disabled,  */
/* does nothing.                                                     */
#if NETWORKING
   struct HISCORE MultiScore[NUMHISCORE],AntiqueScore[NUMHISCORE];
   struct sockaddr_in MetaAddr;
   struct hostent *he;
   int MetaSock;
   gchar *text,*prstr;
   int i;
   if (!MetaServer.Active || !NotifyMetaServer) return;
   if (SendData) {
      g_message(_("Sending data to metaserver at %s\n"),MetaServer.Name);
   } else {
      g_message(_("Notifying metaserver at %s\n"),MetaServer.Name);
   }
   if ((he=gethostbyname(MetaServer.Name))==NULL) {
      g_warning(_("cannot locate metaserver\n"));
      return;
   }
   MetaSock=socket(AF_INET,SOCK_DGRAM,0);
   if (MetaSock==-1) {
      g_warning(_("cannot create socket for metaserver communication\n"));
      return;
   }
   memset(&MetaAddr,0,sizeof(struct sockaddr_in));
   MetaAddr.sin_family=AF_INET;
   MetaAddr.sin_port=htons(MetaServer.UdpPort);
   MetaAddr.sin_addr=*((struct in_addr *)he->h_addr);

   text=g_strdup_printf("report\n%d\n%s\n%d\n%d\n%s",
                        Up ? 1 : 0,VERSION,CountPlayers(FirstServer),
                        MaxClients,MetaServer.Comment);
   if (!SendToMetaServer(Up,MetaSock,text,&MetaAddr)) {
      g_free(text);
      return;
   }
   g_free(text);

   if (SendData) {
      if (HighScoreRead(MultiScore,AntiqueScore)) {
         for (i=0;i<NUMHISCORE;i++) {
            text=g_strdup_printf("multi\n%d\n%s^%s^%s^%c",
                    i,prstr=FormatPrice(MultiScore[i].Money),MultiScore[i].Time,
                    MultiScore[i].Name,MultiScore[i].Dead ? '1':'0');
            g_free(prstr);
            if (!SendToMetaServer(Up,MetaSock,text,&MetaAddr)) {
               g_free(text);
               return;
            }
            g_free(text);
         }
         for (i=0;i<NUMHISCORE;i++) {
            g_free(MultiScore[i].Name); g_free(MultiScore[i].Time);
            g_free(AntiqueScore[i].Name); g_free(AntiqueScore[i].Time);
         }
      } else { g_warning(_("cannot read high score file\n")); }
   }
   CloseSocket(MetaSock);
   MetaTimeout=time(NULL)+METATIME;
#endif /* NETWORKING */
}

void HandleServerPlayer(Player *Play) {
   gchar *buf;
   gboolean MessageRead=FALSE;
   while ((buf=ReadFromConnectionBuffer(Play))!=NULL) {
      MessageRead=TRUE;
      HandleServerMessage(buf,Play);
      g_free(buf);
   }
/* Reset the idle timeout (if necessary) */
   if (MessageRead && IdleTimeout) {
      Play->IdleTimeout=time(NULL)+(time_t)IdleTimeout;
   }
}

void HandleServerMessage(gchar *buf,Player *ReallyFrom) {
/* Given a message "buf" which identifies itself as being from player */
/* "ReallyFrom" by the incoming socket, performs processing and sends */
/* suitable replies.                                                  */
   Player *From,*To,*tmp,*pt;
   GSList *list;
   char Code,*Data,AICode;
   DopeEntry NewEntry;
   int i;
   price_t money;

   if (ProcessMessage(buf,&From,&AICode,&Code,&To,&Data,FirstServer)==-1) {
      g_warning("Bad message");
      return;
   }
   if (From!=ReallyFrom && (From!=&Noone ||
       (Code!=C_NAME && Code!=C_NETMESSAGE))) {
       g_warning(_("Message is lying about its origin\n%s: %c: %s: %s\n"
                 "Should be from %s"),From ? GetPlayerName(From) : "",Code,
                 To ? GetPlayerName(To) : "",Data,
                 ReallyFrom ? GetPlayerName(ReallyFrom) : "NULL");
       g_free(Data);
       return;
   }
   switch(Code) {
      case C_MSGTO:
         if (Network) {
            g_message("%s->%s: %s",GetPlayerName(From),GetPlayerName(To),Data);
         }
         SendServerMessage(From,AICode,Code,To,Data);
         break;
      case C_NETMESSAGE:
         g_message("Net:%s\n",Data);
/*       shutdown(ReallyFrom->fd,SD_RECV);*/
/* Make sure they do actually disconnect, eventually! */
         if (ConnectTimeout) {
            ReallyFrom->ConnectTimeout=time(NULL)+(time_t)ConnectTimeout;
         }
         break;
      case C_NAME:
         pt=GetPlayerByName(Data,FirstServer);
         if (pt && pt!=From) {
            if (ConnectTimeout) {
               ReallyFrom->ConnectTimeout=time(NULL)+(time_t)ConnectTimeout;
            }
            SendServerMessage(NULL,C_NONE,C_NEWNAME,ReallyFrom,NULL);
         } else if (((ReallyFrom && strlen(GetPlayerName(ReallyFrom))==0 &&
                     Network) || (!Network && From==&Noone)) && Data[0]) {
            if (CountPlayers(FirstServer)<MaxClients || !Network) { 
               SendInitialData(ReallyFrom);
               SendMiscData(ReallyFrom);
               if (!Network) {
                  From=g_new(Player,1);
                  FirstServer=AddPlayer(0,From,FirstServer);
               } else From=ReallyFrom;
               SetPlayerName(From,Data);
               for (list=FirstServer;list;list=g_slist_next(list)) {
                  pt=(Player *)list->data;
                  if (pt!=From) {
                     SendServerMessage(NULL,C_NONE,C_LIST,From,
                                       GetPlayerName(pt));
                  }
               }
               SendServerMessage(NULL,C_NONE,C_ENDLIST,From,NULL);
               RegisterWithMetaServer(TRUE,FALSE);
               From->ConnectTimeout=0;

               if (Network) {
                  g_message(_("%s joins the game!"),GetPlayerName(From));
               }
               BroadcastToClients(C_NONE,C_JOIN,GetPlayerName(From),NULL,From);
               From->EventNum=E_ARRIVE;
               SendPlayerData(From);
               SendEvent(From);
            } else {
               From=ReallyFrom;
               g_message(_("MaxClients (%d) exceeded - dropping connection"),
                         MaxClients);
               sprintf(buf,_("Sorry, but this server has a limit of %d "
"%s, which has been reached.^Please try connecting again later."),
                       MaxClients,MaxClients==1 ? _("player") : _("players"));
               SendServerMessage(NULL,C_NONE,C_PRINTMESSAGE,From,buf);
/*             shutdown(From->fd,SD_RECV);*/
/* Make sure they do actually disconnect, eventually! */
               if (ConnectTimeout) {
                  From->ConnectTimeout=time(NULL)+(time_t)ConnectTimeout;
               }
            }
         } else {
            g_message(_("%s will now be known as %s"),GetPlayerName(From),Data);
            BroadcastToClients(C_NONE,C_RENAME,Data,From,From);
            SetPlayerName(From,Data);
         }
         break;
      case C_WANTQUIT:
         if (From->EventNum!=E_FINISH) FinishGame(From,NULL);
         break;
      case C_REQUESTJET:
         i=atoi(Data);
         if (From->EventNum==E_ATTACK || From->EventNum==E_DEFEND ||
             From->EventNum==E_WAITATTACK || From->EventNum==E_FREEFORALL) {
             BreakoffCombat(From,FALSE);
         }
         if (NumTurns>0 && From->Turn>=NumTurns && From->EventNum!=E_FINISH) {
            FinishGame(From,_("Your dealing time is up..."));
         } else if (i!=From->IsAt && (NumTurns==0 || From->Turn<NumTurns) && 
                    From->EventNum==E_NONE && From->Health>0) {
            From->IsAt=(char)i;
            From->Turn++;
            From->Debt=(price_t)((float)From->Debt*1.1);
            From->Bank=(price_t)((float)From->Bank*1.05);
            SendPlayerData(From);
            From->EventNum=E_SUBWAY;
            SendEvent(From);
         } else {
            g_warning(_("%s: DENIED jet to %s"),GetPlayerName(From),
                                                Location[i].Name);
         }
         break;
      case C_REQUESTSCORE:
         SendHighScores(From,FALSE,NULL);
         break;
      case C_CONTACTSPY:
         for (list=FirstServer;list;list=g_slist_next(list)) {
            tmp=(Player *)list->data;
            if (tmp!=From && GetListEntry(&(tmp->SpyList),From)>=0) {
               SendSpyReport(From,tmp);
            }
        }
        break;
      case C_DEPOSIT:
         money=strtoprice(Data);
         if (From->Bank+money >=0 && From->Cash-money >=0) {
            From->Bank+=money; From->Cash-=money;
           SendPlayerData(From);
         }
         break;
      case C_PAYLOAN:
         money=strtoprice(Data);
         if (From->Debt-money >=0 && From->Cash-money >=0) {
            From->Debt-=money; From->Cash-=money;
            SendPlayerData(From);
         }
         break;
      case C_BUYOBJECT:
         BuyObject(From,Data);
         break;
      case C_FIGHTACT:
         if (From->EventNum==E_ATTACK || From->EventNum==E_FREEFORALL) {
            AttackPlayer(From,From->Attacked,
               TotalGunsCarried(From)>0 ? AT_SHOOT : 0);
         } else if (From->EventNum==E_DEFEND) {
            for (list=FirstServer;list;list=g_slist_next(list)) {
               tmp=(Player *)list->data;
               if ((tmp->EventNum==E_FREEFORALL || tmp->EventNum==E_WAITATTACK) 
                   && tmp->Attacked==From) {
                  AttackPlayer(From,tmp,
                               TotalGunsCarried(From)>0 ? AT_SHOOT : 0);
               }
            }
         }
         break;
      case C_ANSWER:
         HandleAnswer(From,To,Data);
         break;
      case C_DONE:
         if (From->EventNum!=E_NONE && From->EventNum<E_OUTOFSYNC) {
            From->EventNum++; SendEvent(From);
         }
         break;
      case C_SPYON:
         if (From->Cash >= Prices.Spy) {
            g_message(_("%s now spying on %s"),GetPlayerName(From),
                                            GetPlayerName(To));
            From->Cash -= Prices.Spy;
            LoseBitch(From,NULL,NULL);
            NewEntry.Play=From; NewEntry.Turns=-1;
            AddListEntry(&(To->SpyList),&NewEntry);
            SendPlayerData(From);
         } else {
            g_warning(_("%s spy on %s: DENIED"),GetPlayerName(From),
                                                GetPlayerName(To));
         }
         break;
      case C_TIPOFF:
         if (From->Cash >= Prices.Tipoff) {
            g_message(_("%s tipped off the cops to %s"),GetPlayerName(From),
                                                        GetPlayerName(To));
            From->Cash -= Prices.Tipoff;
            LoseBitch(From,NULL,NULL);
            NewEntry.Play=From; NewEntry.Turns=0;
            AddListEntry(&(To->TipList),&NewEntry);
            SendPlayerData(From);
         } else {
            g_warning(_("%s tipoff about %s: DENIED"),GetPlayerName(From),
                                                      GetPlayerName(To));
         }
         break;
      case C_SACKBITCH:
         LoseBitch(From,NULL,NULL);
         SendPlayerData(From);
         break;
      case C_MSG:
         if (Network) g_message("%s: %s",GetPlayerName(From),Data);
         BroadcastToClients(C_NONE,C_MSG,Data,From,From);
         break;
      default:
         g_warning("%s:%c:%s:%s",GetPlayerName(From),Code,
                                 GetPlayerName(To),Data);
         break;
   }
   g_free(Data);
}

void ClientLeftServer(Player *Play) {
/* Notifies all clients that player "Play" has left the game and */
/* cleans up after them if necessary.                            */
   Player *tmp;
   GSList *list;
   if (Play->EventNum==E_ATTACK || Play->EventNum==E_DEFEND ||
       Play->EventNum==E_WAITATTACK || Play->EventNum==E_FREEFORALL) {
      BreakoffCombat(Play,TRUE);
   }
   for (list=FirstServer;list;list=g_slist_next(list)) {
      tmp=(Player *)list->data;
      if (tmp!=Play) {
         RemoveAllEntries(&(tmp->TipList),Play);
         RemoveAllEntries(&(tmp->SpyList),Play);
      }
   }
   BroadcastToClients(C_NONE,C_LEAVE,GetPlayerName(Play),Play,Play);
}

void CleanUpServer() {
/* Closes down the server and frees up associated handles and memory */
   if (Server) RegisterWithMetaServer(FALSE,FALSE);
   while (FirstServer) {
      FirstServer=RemovePlayer((Player *)FirstServer->data,FirstServer);
   }
#if NETWORKING
   if (Server) CloseSocket(ListenSock);
#endif
}

void ReregisterHandle(int sig) {
/* Responds to a SIGUSR1 signal, and requests the main event loop to  */
/* reregister the server with the dopewars metaserver.                */
   ReregisterRequest=1;
}

void BreakHandle(int sig) {
/* Traps an attempt by the user to send dopewars a SIGTERM or SIGINT  */
/* (e.g. pressing Ctrl-C) and signals for a "nice" shutdown. Restores */
/* the default signal action (to terminate without cleanup) so that   */
/* the user can still close the program easily if this cleanup code   */
/* then causes problems or long delays.                               */
   struct sigaction sact;
   TerminateRequest=1;
   sact.sa_handler=SIG_DFL;
   sact.sa_flags=0;
   sigaction(SIGTERM,&sact,NULL);
   sigaction(SIGINT,&sact,NULL);
}

void PrintHelpTo(FILE *fp) {
/* Prints the server help screen to the given file pointer */
   int i;
   GString *VarName;
   VarName=g_string_new("");
   fprintf(fp,_(HelpText),VERSION);
   for (i=0;i<NUMGLOB;i++) {
      if (Globals[i].NameStruct[0]) {
         g_string_sprintf(VarName,"%s%s.%s",Globals[i].NameStruct,
                          Globals[i].StructListPt ? "[x]" : "",Globals[i].Name);
      } else {
         g_string_assign(VarName,Globals[i].Name);
      }
      fprintf(fp,"%-26s %s\n",VarName->str,_(Globals[i].Help));
   }
   fprintf(fp,"\n\n");
   g_string_free(VarName,TRUE);
}

void ServerHelp() {
/* Displays a simple help screen listing the server commands and options */
   int i;
#ifdef CYGWIN
   int Lines;
   GString *VarName;
   VarName=g_string_new("");
   g_print(_(HelpText),VERSION);
   Lines=16;
   for (i=0;i<NUMGLOB;i++) {
      if (Globals[i].NameStruct[0]) {
         g_string_sprintf(VarName,"%s%s.%s",Globals[i].NameStruct,
                          Globals[i].StructListPt ? "[x]" : "",Globals[i].Name);
      } else {
         g_string_assign(VarName,Globals[i].Name);
      }
      g_print("%-26s %s\n",VarName->str,_(Globals[i].Help));
      Lines++;
      if (Lines%24==0) {
         g_print(_("--More--")); bgetch(); g_print("\n");
      }
   }
   g_string_free(VarName,TRUE);
#else
   FILE *fp;
   fp=popen(Pager,"w");
   if (fp) {
      PrintHelpTo(fp);
      i=pclose(fp);
      if (i==-1 || (WIFEXITED(i) && WEXITSTATUS(i)==127)) {
         g_warning(_("Pager exited abnormally - using stdout instead..."));
         PrintHelpTo(stdout);
      }
   }
#endif
}

#if NETWORKING
void CreatePidFile() {
/* Creates a pid file (if "PidFile" is non-NULL) and writes the process */
/* ID into it                                                           */
   FILE *fp;
   if (!PidFile) return;
   fp=fopen(PidFile,"w");
   if (fp) {
      g_message(_("Maintaining pid file %s"),PidFile);
      fprintf(fp,"%ld\n",(long)getpid());
      fclose(fp);
      chmod(PidFile,S_IREAD|S_IWRITE);
   } else g_warning(_("Cannot create pid file %s"),PidFile);
}

void RemovePidFile() {
/* Removes the previously-created pid file "PidFile" */
   if (PidFile) unlink(PidFile);
}

gboolean ReadServerKey(GString *LineBuf,gboolean *EndOfLine) {
   int ch;
   *EndOfLine=FALSE;
#ifdef CYGWIN
   ch=bgetch();
   if (ch=='\0') {
      return FALSE;
   } else if (ch=='\r') {
      g_print("\n");
      *EndOfLine=TRUE;
      return TRUE;
   } else if (ch==8) {
      if (strlen(LineBuf->str)>0) {
         g_print("\010 \010");
         g_string_truncate(LineBuf,strlen(LineBuf->str)-1);
      }
      return TRUE;
   }
   g_string_append_c(LineBuf,(gchar)ch);
   g_print("%c",ch);
   return TRUE;
#else
   while (1) {
      ch=getchar();
      if (ch==EOF) return FALSE;
      else if (ch=='\n') {
         *EndOfLine=TRUE;
         break;
      }
      g_string_append_c(LineBuf,(gchar)ch);
   }
   return TRUE;
#endif
}

void StartServer() {
   struct sockaddr_in ServerAddr;
   struct sigaction sact;
   Scanner=g_scanner_new(&ScannerConfig);
   Scanner->input_name="(stdin)";
   if (!CheckHighScoreFile()) {
      g_error(_("Cannot open high score file %s.\n"
                "Either ensure you have permissions to access this file and "
                "directory, or\nspecify an alternate high score file with "
                "the -f command line option."),HiScoreFile);
   }
   CreatePidFile();

/* Make the output line-buffered, so that the log file (if used) is */
/* updated regularly                                                */
   fflush(stdout);

#ifdef SETVBUF_REVERSED /* 2nd and 3rd arguments are reversed on some systems */
   setvbuf(stdout,_IOLBF,(char *)NULL,0);
#else
   setvbuf(stdout,(char *)NULL,_IOLBF,0);
#endif

   Network=TRUE;
   FirstServer=NULL;
   SocketWriteTestPt=NULL;
   ClientMessageHandlerPt=NULL;
   ListenSock=socket(AF_INET,SOCK_STREAM,0);
   if (ListenSock==SOCKET_ERROR) {
      perror("create socket"); exit(1);
   }
   SetReuse(ListenSock);
   fcntl(ListenSock,F_SETFL,O_NONBLOCK);
  
   ServerAddr.sin_family=AF_INET;
   ServerAddr.sin_port=htons(Port);
   ServerAddr.sin_addr.s_addr=INADDR_ANY;
   memset(ServerAddr.sin_zero,0,sizeof(ServerAddr.sin_zero));
   if (bind(ListenSock,(struct sockaddr *)&ServerAddr,
       sizeof(struct sockaddr)) == SOCKET_ERROR) {
      perror("bind socket"); exit(1);
   }

   g_print(_("dopewars server version %s ready and waiting for connections\n"
             "on port %d. For assistance with server commands, enter the "
             "command \"help\"\n"),VERSION,Port);

   if (listen(ListenSock,10)==SOCKET_ERROR) {
      perror("listen socket"); exit(1);
   }

   MetaTimeout=0;

   TerminateRequest=ReregisterRequest=0;

#if !CYGWIN
   sact.sa_handler=ReregisterHandle;
   sact.sa_flags=0;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGUSR1,&sact,NULL)==-1) {
      g_warning(_("Cannot install SIGUSR1 interrupt handler!"));
   }
   sact.sa_handler=BreakHandle;
   sact.sa_flags=0;
   sigemptyset(&sact.sa_mask);
   if (sigaction(SIGINT,&sact,NULL)==-1) {
      g_warning(_("Cannot install SIGINT interrupt handler!"));
   }
   if (sigaction(SIGTERM,&sact,NULL)==-1) {
      g_warning(_("Cannot install SIGTERM interrupt handler!"));
   }
   if (sigaction(SIGHUP,&sact,NULL)==-1) {
      g_warning(_("Cannot install SIGHUP interrupt handler!"));
   }
   sact.sa_handler=SIG_IGN;
   sact.sa_flags=0;
   if (sigaction(SIGPIPE,&sact,NULL)==-1) {
      g_warning(_("Cannot install pipe handler!"));
   }
#endif

   RegisterWithMetaServer(TRUE,TRUE);
}

gboolean HandleServerCommand(char *string) {
   GSList *list;
   Player *tmp;
   g_scanner_input_text(Scanner,string,strlen(string));
   if (!ParseNextConfig(Scanner)) {
      if (strcasecmp(string,"help")==0 || strcasecmp(string,"h")==0 ||
          strcmp(string,"?")==0) {
         ServerHelp();
      } else if (strcasecmp(string,"quit")==0) {
         if (!FirstServer) return TRUE;
         WantQuit=TRUE;
         BroadcastToClients(C_NONE,C_QUIT,NULL,NULL,NULL);
      } else if (strncasecmp(string,"msg:",4)==0) {
         BroadcastToClients(C_NONE,C_MSG,string+4,NULL,NULL);
      } else if (strcasecmp(string,"list")==0) {
         if (FirstServer) {
            g_print(_("Users currently logged on:-\n"));
            for (list=FirstServer;list;list=g_slist_next(list)) {
               tmp=(Player *)list->data;
               g_print("%s\n",GetPlayerName(tmp));
            }
         } else g_message(_("No users currently logged on!"));
      } else if (strncasecmp(string,"push ",5)==0) {
         tmp=GetPlayerByName(string+5,FirstServer);
         if (tmp) {
            g_message(_("Pushing %s"),GetPlayerName(tmp));
            SendServerMessage(NULL,C_NONE,C_PUSH,tmp,NULL);
         } else g_warning(_("No such user!"));
      } else if (strncasecmp(string,"kill ",5)==0) {
         tmp=GetPlayerByName(string+5,FirstServer);
         if (tmp) {
            g_message(_("%s killed"),GetPlayerName(tmp));
            BroadcastToClients(C_NONE,C_KILL,GetPlayerName(tmp),tmp,
                               (Player *)FirstServer->data);
            FirstServer=RemovePlayer(tmp,FirstServer);
         } else g_warning(_("No such user!"));
      } else {
         g_warning(_("Unknown command - try \"help\" for help..."));
      }
   }
   return FALSE;
}

void HandleNewConnection() {
   int cadsize;
   int ClientSock;
   struct sockaddr_in ClientAddr;
   Player *tmp;
   cadsize=sizeof(struct sockaddr);
   if ((ClientSock=accept(ListenSock,(struct sockaddr *)&ClientAddr,
       &cadsize))==-1) {
      perror("accept socket"); bgetch(); exit(1);
   }
   fcntl(ClientSock,F_SETFL,O_NONBLOCK);
   g_message(_("got connection from %s"),inet_ntoa(ClientAddr.sin_addr));
   tmp=g_new(Player,1);
   FirstServer=AddPlayer(ClientSock,tmp,FirstServer);
   if (ConnectTimeout) {
      tmp->ConnectTimeout=time(NULL)+(time_t)ConnectTimeout;
   }
}

void StopServer() {
   g_scanner_destroy(Scanner);
   CleanUpServer();
   RemovePidFile();
}

gboolean RemovePlayerFromServer(Player *Play,gboolean WantQuit) {
   if (!WantQuit && strlen(GetPlayerName(Play))>0) {
      g_message(_("%s leaves the server!"),GetPlayerName(Play));
      ClientLeftServer(Play);
/* Blank the name, so that CountPlayers ignores this player */
      SetPlayerName(Play,NULL);
/* Report the new high scores (if any) and the new number of players
   to the metaserver */
      RegisterWithMetaServer(TRUE,TRUE);
   }
   FirstServer=RemovePlayer(Play,FirstServer);
   return (!FirstServer && WantQuit);
}

void ServerLoop() {
/* Initialises server, processes network and interactive messages, and */
/* finally cleans up the server on exit.                               */
   Player *tmp;
   GSList *list,*nextlist;
   fd_set readfs,writefs,errorfs;
   int topsock;
   char WantQuit=FALSE;
   char InputClosed=FALSE;
   struct timeval timeout;
   int MinTimeout;
   GString *LineBuf;
   gboolean EndOfLine;

   StartServer();

   LineBuf=g_string_new("");
   while (1) {
      FD_ZERO(&readfs);
      FD_ZERO(&writefs);
      FD_ZERO(&errorfs);
      if (!InputClosed) FD_SET(0,&readfs);
      FD_SET(ListenSock,&readfs);
      FD_SET(ListenSock,&errorfs);
      topsock=ListenSock+1;
      for (list=FirstServer;list;list=g_slist_next(list)) {
         tmp=(Player *)list->data;
         if (tmp->fd>0) {
            FD_SET(tmp->fd,&readfs);
            if (tmp->WriteBuf.DataPresent) FD_SET(tmp->fd,&writefs);
            FD_SET(tmp->fd,&errorfs);
            if (tmp->fd>=topsock) topsock=tmp->fd+1;
         }
      }
      MinTimeout=GetMinimumTimeout(FirstServer);
      if (MinTimeout!=-1) {
         timeout.tv_sec=MinTimeout;
         timeout.tv_usec=0;
      }
      if (bselect(topsock,&readfs,&writefs,&errorfs,
                 MinTimeout==-1 ? NULL : &timeout)==-1) {
         if (errno==EINTR) {
            if (ReregisterRequest) {
               ReregisterRequest=0;
               RegisterWithMetaServer(TRUE,TRUE);
               continue;
            } else if (TerminateRequest) break; else continue;
         }
         perror("select"); bgetch(); break;
      }
      FirstServer=HandleTimeouts(FirstServer);
      if (FD_ISSET(0,&readfs)) {
         if (ReadServerKey(LineBuf,&EndOfLine)==FALSE) {
            if (isatty(0)) {
               break;
            } else {
               g_message(_("Standard input closed."));
               InputClosed=TRUE;
            }
         } else if (EndOfLine) {
            if (HandleServerCommand(LineBuf->str)) break;
            g_string_truncate(LineBuf,0);
         }
      }
      if (FD_ISSET(ListenSock,&readfs)) {
         HandleNewConnection();
      }
      list=FirstServer;
      while (list) {
         nextlist=g_slist_next(list);
         tmp=(Player *)list->data;
         if (tmp && FD_ISSET(tmp->fd,&errorfs)) {
            g_warning("socket error from client: %d",tmp->fd);
            CleanUpServer(); bgetch(); break;
         }
         if (tmp && FD_ISSET(tmp->fd,&writefs)) {
/* Try and empty the player's write buffer */
            if (!WriteConnectionBufferToWire(tmp)) {
/* The socket has been shut down, or the buffer was filled - remove player */
               if (RemovePlayerFromServer(tmp,WantQuit)) break;
               tmp=NULL;
            }
         }
         if (tmp && FD_ISSET(tmp->fd,&readfs)) {
/* Read any waiting data into the player's read buffer */
            if (!ReadConnectionBufferFromWire(tmp)) {
/* remove player! */
               if (RemovePlayerFromServer(tmp,WantQuit)) break;
               tmp=NULL;
            } else {
/* If any complete messages were read, process them */
               HandleServerPlayer(tmp);
            }
         }
         list=nextlist;
      }
   }
   StopServer();
   g_string_free(LineBuf,TRUE);
}
#endif /* NETWORKING */

void FinishGame(Player *Play,char *Message) {
/* Tells player "Play" that the game is over; display "Message" */
   ClientLeftServer(Play);
   Play->EventNum=E_FINISH;
   SendHighScores(Play,TRUE,Message);
/* shutdown(Play->fd,SD_RECV);*/
/* Make sure they do actually disconnect, eventually! */
   if (ConnectTimeout) {
      Play->ConnectTimeout=time(NULL)+(time_t)ConnectTimeout;
   }
}

void HighScoreTypeRead(struct HISCORE *HiScore,FILE *fp) {
/* Reads a batch of NUMHISCORE high scores into "HiScore" from "fp" */
   int i;
   char *buf;
   for (i=0;i<NUMHISCORE;i++) {
      if (read_string(fp,&HiScore[i].Name)==EOF) break;
      read_string(fp,&HiScore[i].Time);
      read_string(fp,&buf);
      HiScore[i].Money=strtoprice(buf); g_free(buf);
      HiScore[i].Dead=fgetc(fp);
   }
}

void HighScoreTypeWrite(struct HISCORE *HiScore,FILE *fp) {
/* Writes out a batch of NUMHISCORE high scores from "HiScore" to "fp" */
   int i;
   gchar *text;
   for (i=0;i<NUMHISCORE;i++) {
      if (HiScore[i].Name) {
         fwrite(HiScore[i].Name,strlen(HiScore[i].Name)+1,1,fp);
      } else fputc(0,fp);
      if (HiScore[i].Time) {
         fwrite(HiScore[i].Time,strlen(HiScore[i].Time)+1,1,fp);
      } else fputc(0,fp);
      text=pricetostr(HiScore[i].Money);
      fwrite(text,strlen(text)+1,1,fp);
      g_free(text);
      fputc(HiScore[i].Dead,fp);
   }
}

gboolean CheckHighScoreFile() {
/* Tests to see whether the high score file is is read-and-writable        */
   FILE *fp;
   fp=fopen(HiScoreFile,"a+");
   if (fp) {
      fclose(fp);
      return TRUE;
   } else {
      return FALSE;
   }
}

int HighScoreRead(struct HISCORE *MultiScore,struct HISCORE *AntiqueScore) {
/* Reads all the high scores into MultiScore and                           */
/* AntiqueScore (antique mode scores). Returns 1 on success, 0 on failure. */
   FILE *fp;
   memset(MultiScore,0,sizeof(struct HISCORE)*NUMHISCORE);
   memset(AntiqueScore,0,sizeof(struct HISCORE)*NUMHISCORE);
   fp=fopen(HiScoreFile,"r");
   if (fp) {
      HighScoreTypeRead(AntiqueScore,fp);
      HighScoreTypeRead(MultiScore,fp);
      fclose(fp);
   } else return 0;
   return 1;
}

int HighScoreWrite(struct HISCORE *MultiScore,struct HISCORE *AntiqueScore) {
/* Writes out all the high scores from MultiScore and AntiqueScore; returns */
/* 1 on success, 0 on failure.                                              */
   FILE *fp;
   fp=fopen(HiScoreFile,"w");
   if (fp) {
      HighScoreTypeWrite(AntiqueScore,fp);
      HighScoreTypeWrite(MultiScore,fp);
      fclose(fp);
   } else return 0;
   return 1;
}

void SendHighScores(Player *Play,char EndGame,char *Message) {
/* Adds "Play" to the high score list if necessary, and then sends the */
/* scores over the network to "Play"                                   */
/* If "EndGame" is TRUE, add the current score if it's high enough and */
/* display an explanatory message. "Message" is tacked onto the start  */
/* if it's non-NULL. The client is then informed that the game's over. */
   struct HISCORE MultiScore[NUMHISCORE],AntiqueScore[NUMHISCORE],Score;
   struct HISCORE *HiScore;
   struct tm *timep;
   time_t tim;
   GString *text;
   int i,j,InList=-1;
   text=g_string_new("");
   if (!HighScoreRead(MultiScore,AntiqueScore)) {
      g_warning(_("Unable to read high score file %s"),HiScoreFile);
   }
   if (Message) {
      g_string_assign(text,Message);
      if (strlen(text->str)>0) g_string_append_c(text,'^');
   }
   if (WantAntique) HiScore=AntiqueScore; else HiScore=MultiScore;
   if (EndGame) {
      Score.Money=Play->Cash+Play->Bank-Play->Debt;
      Score.Name=g_strdup(GetPlayerName(Play));
      if (Play->Health==0) Score.Dead=1; else Score.Dead=0;
      tim=time(NULL);
      timep=gmtime(&tim);
      Score.Time=g_new(char,80); /* Yuck! */
      strftime(Score.Time,80,"%d-%m-%Y",timep);
      for (i=0;i<NUMHISCORE;i++) {
         if (InList==-1 && (Score.Money > HiScore[i].Money ||
             !HiScore[i].Time || HiScore[i].Time[0]==0)) {
            InList=i;
            g_string_append(text,
                            _("Congratulations! You made the high scores!"));
            SendPrintMessage(NULL,C_NONE,Play,text->str);
            g_free(HiScore[NUMHISCORE-1].Name);
            g_free(HiScore[NUMHISCORE-1].Time);
            for (j=NUMHISCORE-1;j>i;j--) {
               memcpy(&HiScore[j],&HiScore[j-1],sizeof(struct HISCORE));
            }
            memcpy(&HiScore[i],&Score,sizeof(struct HISCORE));
            break;
         }
      }
      if (InList==-1) {
         g_string_append(text,
                         _("You didn't even make the high score table..."));
         SendPrintMessage(NULL,C_NONE,Play,text->str);
      }
   }
   SendServerMessage(NULL,C_NONE,C_STARTHISCORE,Play,NULL);

   j=0;
   for (i=0;i<NUMHISCORE;i++) {
      if (SendSingleHighScore(Play,&HiScore[i],j,InList==i)) j++;
   }
   if (InList==-1 && EndGame) SendSingleHighScore(Play,&Score,j,1);
   SendServerMessage(NULL,C_NONE,C_ENDHISCORE,Play,EndGame ? "end" : NULL);
   if (!EndGame) SendDrugsHere(Play,FALSE);
   if (EndGame && !HighScoreWrite(MultiScore,AntiqueScore)) {
      g_warning(_("Unable to write high score file %s"),HiScoreFile);
   }
   for (i=0;i<NUMHISCORE;i++) {
      g_free(MultiScore[i].Name); g_free(MultiScore[i].Time);
      g_free(AntiqueScore[i].Name); g_free(AntiqueScore[i].Time);
   }
   g_string_free(text,TRUE);
}

int SendSingleHighScore(Player *Play,struct HISCORE *Score,
                        int index,char Bold) {
/* Sends a single high score in "Score" with position "index" to player  */
/* "Play". If Bold is TRUE, instructs the client to display the score in */
/* bold text.                                                            */
   gchar *Data,*prstr;
   if (!Score->Time || Score->Time[0]==0) return 0;
   Data=g_strdup_printf("%d^%c%c%18s  %-14s %-34s %8s%c",index,
                        Bold ? 'B' : 'N',Bold ? '>' : ' ',
                        prstr=FormatPrice(Score->Money),
                        Score->Time,Score->Name,Score->Dead ? _("(R.I.P.)") :"",
                        Bold ? '<' : ' ');
   SendServerMessage(NULL,C_NONE,C_HISCORE,Play,Data);
   g_free(prstr); g_free(Data);
   return 1;
}

void SendEvent(Player *To) {
/* In order for the server to keep track of the state of each client, each    */
/* client's state is identified by its EventNum data member. So, for example, */
/* there is a state for fighting the cops, a state for going to the bank, and */
/* so on. This function instructs client player "To" to carry out the actions */
/* expected of it in its current state. It is the client's responsibility to  */
/* ensure that it carries out the correct actions to advance itself to the    */
/* "next" state; if it fails in this duty it will hang!                       */
   price_t Money;
   int i,j;
   gchar *text;
   Player *Play;
   GSList *list;
   gchar *prstr;
   if (!To) return;
   if (To->EventNum==E_MAX) To->EventNum=E_NONE;
   if (To->EventNum==E_NONE || To->EventNum>=E_OUTOFSYNC) return;
   Money=To->Cash+To->Bank-To->Debt;

   ClearPrices(To);

   while (To->EventNum<E_MAX) {
      switch (To->EventNum) {
         case E_SUBWAY:
            SendServerMessage(NULL,C_NONE,C_SUBWAYFLASH,To,NULL);
            break;
         case E_OFFOBJECT:
            for (i=0;i<To->TipList.Number;i++) {
               g_message(_("%s: Tipoff from %s"),GetPlayerName(To),
                         GetPlayerName(To->TipList.Data[i].Play));
               To->OnBehalfOf=To->TipList.Data[i].Play;
               SendCopOffer(To,FORCECOPS);
               return;
            }
            for (i=0;i<To->SpyList.Number;i++) {
               if (To->SpyList.Data[i].Turns<0) {
                  To->OnBehalfOf=To->SpyList.Data[i].Play;
                  SendCopOffer(To,FORCEBITCH);
                  return;
               }
               To->SpyList.Data[i].Turns++;
               if (To->SpyList.Data[i].Turns>3 && 
                   brandom(0,100)<10+To->SpyList.Data[i].Turns) {
                   if (TotalGunsCarried(To) > 0) j=brandom(0,NUMDISCOVER);
                   else j=brandom(0,NUMDISCOVER-1);
                   text=g_strdup_printf(_("One of your %s was spying for %s."
                                          "^The spy %s!"),Names.Bitches,
                                        GetPlayerName(To->SpyList.Data[i].Play),
                                        _(Discover[j]));
                   if (j!=DEFECT) LoseBitch(To,NULL,NULL);
                   SendPlayerData(To);
                   SendPrintMessage(NULL,C_NONE,To,text);
                   g_free(text);
                   text=g_strdup_printf(_("Your spy working with %s has "
                                          "been discovered!^The spy %s!"),
                                        GetPlayerName(To),_(Discover[j]));
                   if (j==ESCAPE) GainBitch(To->SpyList.Data[i].Play);
                   To->SpyList.Data[i].Play->Flags &= ~SPYINGON;
                   SendPlayerData(To->SpyList.Data[i].Play);
                   SendPrintMessage(NULL,C_NONE,
                                     To->SpyList.Data[i].Play,text);
                   g_free(text);
                   RemoveListEntry(&(To->SpyList),i);
                   i--;
               }
            }
            if (Money>3000000) i=130;   
            else if (Money>1000000) i=115;   
            else i=100;   
            if (brandom(0,i)>75) {
               if (SendCopOffer(To,NOFORCE)) return;
            }
            break;
         case E_SAYING:
            if (!Sanitized && (brandom(0,100) < 15)) {
               if (brandom(0,100)<50) {
                  text=g_strdup_printf(_(" The lady next to you on the subway "
                   "said,^ \"%s\"%s"),
                   SubwaySaying[brandom(0,NumSubway)],brandom(0,100)<30 ? 
                   _("^    (at least, you -think- that's what she said)") : "");
               } else {
                  text=g_strdup_printf(_(" You hear someone playing %s"),
                                       Playing[brandom(0,NumPlaying)]);
               }
               SendPrintMessage(NULL,C_NONE,To,text);
               g_free(text);
            }
            break;
         case E_LOANSHARK:
            if (To->IsAt+1==LoanSharkLoc && To->Debt>0) {
               text=g_strdup_printf(_("YN^Would you like to visit %s?"),
                                    Names.LoanSharkName);
               SendQuestion(NULL,C_ASKLOAN,To,text);
               g_free(text);
               return;
            }
            break;
         case E_BANK:
            if (To->IsAt+1==BankLoc) {
               text=g_strdup_printf(_("YN^Would you like to visit %s?"),
                                    Names.BankName);
               SendQuestion(NULL,C_ASKBANK,To,text);
               g_free(text);
               return;
            }
            break;
         case E_GUNSHOP:
            if (To->IsAt+1==GunShopLoc && !Sanitized && !WantAntique) {
               text=g_strdup_printf(_("YN^Would you like to visit %s?"),
                                    Names.GunShopName);
               SendQuestion(NULL,C_ASKGUNSHOP,To,text);
               g_free(text);
               return;
            }
            break;
         case E_ROUGHPUB:
            if (To->IsAt+1==RoughPubLoc && !WantAntique) {
               text=g_strdup_printf(_("YN^Would you like to visit %s?"),
                                    Names.RoughPubName);
               SendQuestion(NULL,C_ASKPUB,To,text);
               g_free(text);
               return;
            }
            break;
         case E_HIREBITCH:
            if (To->IsAt+1==RoughPubLoc && !WantAntique) {
               To->Bitches.Price=brandom(Bitch.MinPrice,Bitch.MaxPrice);
               text=g_strdup_printf(
                       _("YN^^Would you like to hire %s %s for %s?"),
                       StartsWithVowel(Names.Bitch) ? _("an") : _("a"),
                       Names.Bitch,prstr=FormatPrice(To->Bitches.Price));
               SendQuestion(NULL,C_ASKBITCH,To,text);
               g_free(text); g_free(prstr);
               return;
            }
            break;
         case E_ARRIVE:
            for (list=FirstServer;list;list=g_slist_next(list)) {
               Play=(Player *)list->data;
               if (Play!=To && Play->IsAt==To->IsAt &&
                   Play->EventNum==E_NONE && TotalGunsCarried(To)>0) {
                  text=g_strdup_printf(_("AE^%s is already here!^"
                                         "Do you Attack, or Evade?"),
                                       GetPlayerName(Play));
                  To->Attacked=Play;
                  SendDrugsHere(To,TRUE);
                  SendQuestion(NULL,C_MEETPLAYER,To,text);
                  g_free(text);
                  return;
               }
            }
            SendDrugsHere(To,TRUE);
            break;
      }
      To->EventNum++;
   } 
   if (To->EventNum >= E_MAX) To->EventNum=E_NONE;
}

int SendCopOffer(Player *To,char Force) {
/* In response to client player "To" being in state E_OFFOBJECT,   */
/* randomly engages the client in combat with the cops or offers   */
/* other random events. Returns 0 if the client should then be     */
/* advanced to the next state, 1 otherwise (i.e. if there are      */
/* questions pending which the client must answer first)           */
/* If Force==FORCECOPS, engage in combat with the cops for certain */
/* If Force==FORCEBITCH, offer the client a bitch for certain      */
   int i;
   i=brandom(0,100);
   if (Force==FORCECOPS) i=100;
   else if (Force==FORCEBITCH) i=0;
   else To->OnBehalfOf=NULL;
   if (i<33) { 
      return(OfferObject(To,Force==FORCEBITCH)); 
   } else if (i<50) { return(RandomOffer(To));
   } else if (Sanitized) { return 0;
   } else {
      StartOfficerHardass(To,To->EventNum+1,NULL,NULL);
   }
   return 1;
}

void StartOfficerHardass(Player *Play,int ResyncNum,
                         char *LoneMessage,char *DeputyMessage) {
/* Starts combat between player "Play" and the cops. "ResyncNum" is */
/* the event number to be returned to after combat is complete.     */
/* "LoneMessage" and "DeputyMessage" are the format strings passed  */
/* to OfficerHardass if they are non-NULL.                          */
   price_t Money;
   if (!Play) return;
   Money=Play->Cash+Play->Bank-Play->Debt;
   if (Money>3000000) Play->Cops=brandom(11,27);
   else if (Money>1000000) Play->Cops=brandom(7,14);
   else if (Money>500000) Play->Cops=brandom(6,12);
   else if (Money>100000) Play->Cops=brandom(2,8);
   else Play->Cops=brandom(1,5);

   Play->ResyncNum=ResyncNum;
   Play->EventNum=E_COPS;
   if (Play->ResyncNum==E_MAX || Play->ResyncNum==E_NONE) {
      SendServerMessage(NULL,C_NONE,C_CHANGEDISP,Play,"N");
   }
   OfficerHardass(Play,LoneMessage,DeputyMessage);
}

void OfficerHardass(Player *Play,char *LoneMessage,char *DeputyMessage) {
/* Send client "Play" a message announcing the attack of the cops */
/* The format string used for this purpose can be altered by      */
/* passing non-NULL "LoneMessage" (for unaccompanied Officer      */
/* Hardass) and/or "DeputyMessage" (for him with x deputies)      */
   char LoneDefault[] = { N_("YN^Officer %s is chasing you!") };
   char DeputyDefault[] = { 
      N_("YN^Officer %s and %d of his deputies are chasing you!")
   };
   char *OfficerName;
   GString *text;
   
   if (!Play || Play->EventNum!=E_COPS) return;
   if (Play->Cops==0) { 
      Play->EventNum=Play->ResyncNum; SendEvent(Play); return; 
   }
   text=g_string_new(NULL);
   OfficerName=(Play->Flags&DEADHARDASS ? Names.ReserveOfficer : 
                                          Names.Officer);
   if (Play->Cops==1) {
      g_string_sprintf(text,LoneMessage ? LoneMessage : _(LoneDefault),
                       OfficerName);
   } else {
      g_string_sprintf(text,DeputyMessage ? DeputyMessage : _(DeputyDefault),
	               OfficerName, Play->Cops - 1);
   }
   SendPlayerData(Play);
   if (TotalGunsCarried(Play)==0) {
      g_string_append(text,_("^Do you run?"));
      SendQuestion(NULL,C_ASKRUN,Play,text->str);
   } else {
      g_string_append(text,_("^Do you Run, or Fight?"));
      if (strlen(text->str)>=2) {
         text->str[0]='R'; text->str[1]='F';
      }
      SendQuestion(NULL,C_ASKRUNFIGHT,Play,text->str);
   }
   g_string_free(text,TRUE);
}

void FinishFightWithHardass(Player *Play,char *Message) {
/* Clean up after a fight between "Play" and the cops. If the cops were */
/* tipped off by another player, inform them of the results.            */
/* If the player died, pass "Message" to the FinishGame subroutine.     */
   GString *text;
   if (g_slist_find(FirstServer,(gpointer)Play->OnBehalfOf)) {
      g_message(_("%s: tipoff by %s finished OK."),GetPlayerName(Play),
                GetPlayerName(Play->OnBehalfOf));
      RemoveListPlayer(&(Play->TipList),Play->OnBehalfOf);
      text=g_string_new(NULL);
      if (Play->Health==0) {
         g_string_sprintf(text,
           _("Following your tipoff, the cops ambushed %s, who was shot dead"),
           GetPlayerName(Play));
      } else {
         g_string_sprintf(text,
                _("Following your tipoff, the cops ambushed %s, who escaped "
                "with %d %s. "),GetPlayerName(Play),
                Play->Bitches.Carried,Names.Bitches);
      }
      GainBitch(Play->OnBehalfOf);
      SendPlayerData(Play->OnBehalfOf);
      SendPrintMessage(NULL,C_NONE,Play->OnBehalfOf,text->str);
      g_string_free(text,TRUE);
   }
   Play->OnBehalfOf=NULL;
   if (Play->Health==0) FinishGame(Play,Message);
   else {
      Play->EventNum=Play->ResyncNum; 
      if (Play->ResyncNum==E_MAX || Play->ResyncNum==E_NONE) {
         SendServerMessage(NULL,C_NONE,C_CHANGEDISP,Play,"Y");
      }
      SendEvent(Play);
   }
}

void FireAtHardass(Player *Play,char FireType) {
/* Have player "Play" attack the cops.                   */
/* FireType is F_STAND: Player has no gun and didn't run */
/*             F_RUN:   Player chose to run              */
/*             F_FIGHT: Player chose to fire back        */
   int Damage,i,j;
   char *OfficerName;
   gchar *prstr;
   GString *text;

   if (!Play || Play->EventNum!=E_COPS) return;
   if (Play->Cops==0) { FinishFightWithHardass(Play,NULL); return; }
   
   text=g_string_new("^");
   OfficerName=(Play->Flags&DEADHARDASS ? Names.ReserveOfficer : 
                                          Names.Officer);
   if (FireType==F_STAND) {
      g_string_append(text,_("^You stand there like an idiot."));
   } else if (FireType==F_RUN) {
      if (brandom(0,100) < Cops.EscapeProb-(Play->Cops-1)*Cops.DeputyEscape) {
         if (Play->Cops==1) {
            g_string_append(text,_("^You lose him in the alleys."));
         } else {
            g_string_append(text,_("^You lose them in the alleys."));
         }
         SendPrintMessage(NULL,C_NONE,Play,text->str);
         FinishFightWithHardass(Play,NULL);
         g_string_free(text,TRUE);
         return;
      } else {
         if (Play->Cops==1) {
            g_string_append(text,_("^You can\'t shake him, man!"));
         } else {
            g_string_append(text,_("^You can\'t shake them, man!"));
         }
      }
   } else if (FireType==F_FIGHT) {
      Damage=100-brandom(0,Play->Cops)*Cops.Toughness;
      for (i=0;i<NumGun;i++) for (j=0;j<Play->Guns[i].Carried;j++) {
         Damage+=brandom(0,Gun[i].Damage);
      }
      if (Damage>=100) {
         if (Play->Cops==1) {
            i=brandom(1500,3000);
            g_string_sprintfa(text,_("^You killed Officer %s! "
                              "You find %s on his corpse!"),
                              OfficerName,prstr=FormatPrice(i));
            g_free(prstr);
            Play->Cash += i;
            Play->Flags |= DEADHARDASS;
            SendPlayerData(Play);
            SendPrintMessage(NULL,C_NONE,Play,text->str);
            Play->DocPrice=brandom(1000,2000-5*Play->Health);
            if (brandom(0,100)<75 && Play->DocPrice<=Play->Cash &&
                Play->Health<100) {
               Play->EventNum=E_DOCTOR;
	       if (Play->Bitches.Carried && !WantAntique) {
                  g_string_sprintf(text,
                         _("YN^^^^Do you pay a doctor %s to sew your %s up?"),
                        prstr=FormatPrice(Play->DocPrice),Names.Bitches);
               } else {
                  g_string_sprintf(text,
                         _("YN^^^^Do you pay a doctor %s to sew you up?"),
                        prstr=FormatPrice(Play->DocPrice));
               }
               g_free(prstr);
               SendQuestion(NULL,C_ASKSEW,Play,text->str);
            } else {
               FinishFightWithHardass(Play,NULL);
            }
            g_string_free(text,TRUE);
            return;
         } else {
            g_string_append(text,_("^You got one, man!"));
            Play->Cops--;
         }
      } else g_string_append(text,_("^You missed!"));
   }

   if (Play->Cops==1) {
      g_string_append(text,_("^He's firing on you, man! "));
   } else {
      g_string_append(text,_("^They're firing on you, man! "));
   }
   if (brandom(0,100) < Cops.HitProb+(Play->Cops-1)*Cops.DeputyHit) {
      g_string_append(text,_("You've been hit! "));
      Damage=0;
      for (i=0;i<Play->Cops;i++) Damage+=brandom(0,Cops.Damage);
      if (Damage==0) Damage=1;
      if (Damage>Play->Health) {
         if (Play->Bitches.Carried==0 || WantAntique) {
            if (Play->Cops==1) {
               g_string_append(text,_("He wasted you, man! What a drag!"));
            } else {
               g_string_append(text,_("They wasted you, man! What a drag!"));
            }
            Play->Health=0;
            SendPlayerData(Play);
            FinishFightWithHardass(Play,text->str);
            g_string_free(text,TRUE);
            return;
         } else {
            g_string_sprintfa(text,_("You lost one of your %s!"),Names.Bitches);
            LoseBitch(Play,NULL,NULL);
            Play->Health=100;
         }
      } else {
         Play->Health-=Damage;
      }
   } else {
      if (Play->Cops==1) {
         g_string_append(text,_("He missed!"));
      } else {
         g_string_append(text,_("They missed!"));
      }
   }
   SendPlayerData(Play);
   SendPrintMessage(NULL,C_NONE,Play,text->str);
   g_string_free(text,TRUE);
   OfficerHardass(Play,NULL,NULL);
}

int RandomOffer(Player *To) {
/* Inform player "To" of random offers or happenings. Returns 0 if */
/* the client can immediately be advanced to the next state, or 1  */
/* there are first questions to be answered.                       */
   int r,amount,ind;
   GString *text;
   r=brandom(0,100);

   text=g_string_new(NULL);

   if (!Sanitized && (r < 10)) {
      g_string_assign(text,_("You were mugged in the subway!"));
      To->Cash=To->Cash*(price_t)brandom(80,95)/100l;
   } else if (r<30) {
      amount=brandom(3,7);
      ind=IsCarryingRandom(To,amount);
      if (ind==-1 && amount>To->CoatSize) {
         g_string_free(text,TRUE); return 0;
      }
      if (ind==-1) {
         ind=brandom(0,NumDrug);
         g_string_sprintf(text,
            _("You meet a friend! He gives you %d %s."),amount,Drug[ind].Name);
         To->Drugs[ind].Carried+=amount;
         To->CoatSize-=amount;
      } else {
         g_string_sprintf(text,
             _("You meet a friend! You give him %d %s."),amount,Drug[ind].Name);
         To->Drugs[ind].Carried-=amount;
         To->CoatSize+=amount;
      }
      SendPlayerData(To);
      SendPrintMessage(NULL,C_NONE,To,text->str);
   } else if (Sanitized) {
      g_message(_("Sanitized away a RandomOffer"));
   } else if (r<50) {
      amount=brandom(3,7);
      ind=IsCarryingRandom(To,amount);
      if (ind!=-1) {
         g_string_sprintf(text,_("Police dogs chase you for %d blocks! "
                          "You dropped some %s! That's a drag, man!"),
                          brandom(3,7),Names.Drugs);
         To->Drugs[ind].Carried-=amount;
         To->CoatSize+=amount;
         SendPlayerData(To);
         SendPrintMessage(NULL,C_NONE,To,text->str);
      } else {
         ind=brandom(0,NumDrug);
         amount=brandom(3,7);
         if (amount>To->CoatSize) {
            g_string_free(text,TRUE); return 0;
         }
         g_string_sprintf(text,
                          _("You find %d %s on a dead dude in the subway!"),
                          amount,Drug[ind].Name);
         To->Drugs[ind].Carried+=amount;
         To->CoatSize-=amount;
         SendPlayerData(To);
         SendPrintMessage(NULL,C_NONE,To,text->str);
      }
   } else if (r<60 && To->Drugs[WEED].Carried+To->Drugs[HASHISH].Carried>0) {
      ind = (To->Drugs[WEED].Carried>To->Drugs[HASHISH].Carried) ?
            WEED : HASHISH;
      amount=brandom(2,6);
      if (amount>To->Drugs[ind].Carried) amount=To->Drugs[ind].Carried;
      g_string_sprintf(text,_("Your mama made brownies with some of your %s! "
                       "They were great!"),Drug[ind].Name);
      To->Drugs[ind].Carried-=amount;
      To->CoatSize+=amount;
      SendPlayerData(To);
      SendPrintMessage(NULL,C_NONE,To,text->str);
   } else if (r<65) {
      g_string_assign(text,
                      _("YN^There is some weed that smells like paraquat here!"
                      "^It looks good! Will you smoke it? "));
      To->EventNum=E_WEED;
      SendQuestion(NULL,C_NONE,To,text->str);
      g_string_free(text,TRUE);
      return 1;
   } else {
      g_string_sprintf(text,_("You stopped to %s."),
                       StoppedTo[brandom(0,NumStoppedTo)]);
      amount=brandom(1,10);
      if (To->Cash>=amount) To->Cash-=amount;
      SendPlayerData(To);
      SendPrintMessage(NULL,C_NONE,To,text->str);
   }
   g_string_free(text,TRUE);
   return 0;
}

int OfferObject(Player *To,char ForceBitch) {
/* Offers player "To" bitches/trenchcoats or guns. If ForceBitch is  */
/* TRUE, then a bitch is definitely offered. Returns 0 if the client */
/* can advance immediately to the next state, 1 otherwise.           */
   int ObjNum;
   gchar *prstr,*text=NULL;

   if (brandom(0,100)<50 || ForceBitch) {
      if (WantAntique) {
         To->Bitches.Price=brandom(MINTRENCHPRICE,MAXTRENCHPRICE);
         text=g_strdup_printf(_("Would you like to buy a bigger trenchcoat "
                              "for %s?"),prstr=FormatPrice(To->Bitches.Price));
         g_free(prstr);
      } else {
         To->Bitches.Price=brandom(Bitch.MinPrice,Bitch.MaxPrice)/10l;
         text=g_strdup_printf(_("YN^Hey dude! I'll help carry your %s for a "
                              "mere %s. Yes or no?"),Names.Drugs,
                              prstr=FormatPrice(To->Bitches.Price));
         g_free(prstr);
      }
      SendQuestion(NULL,C_ASKBITCH,To,text);
      g_free(text);
      return 1;
   } else if (!Sanitized && (TotalGunsCarried(To) < To->Bitches.Carried+2)) {
      ObjNum=brandom(0,NumGun);
      To->Guns[ObjNum].Price=Gun[ObjNum].Price/10;
      if (Gun[ObjNum].Space>To->CoatSize) return 0;
      text=g_strdup_printf(_("YN^Would you like to buy a %s for %s?"),
                           Gun[ObjNum].Name,
                           prstr=FormatPrice(To->Guns[ObjNum].Price));
      g_free(prstr);
      SendQuestion(NULL,C_ASKGUN,To,text);
      g_free(text);
      return 1;
   }
   return 0;
}

void SendDrugsHere(Player *To,char DisplayBusts) {
/* Sends details of drug prices to player "To". If "DisplayBusts"   */
/* is TRUE, also regenerates drug prices and sends details of       */
/* special events such as drug busts                                */
   int i;
   gchar *Deal,*prstr;
   GString *text;
   gboolean First;

   Deal=g_malloc0(NumDrug);
   if (DisplayBusts) GenerateDrugsHere(To,Deal);

   text=g_string_new(NULL);
   First=TRUE;
   if (DisplayBusts) for (i=0;i<NumDrug;i++) if (Deal[i]) {
      if (!First) g_string_append_c(text,'^');
      if (Drug[i].Expensive) {
         g_string_sprintfa(text,Deal[i]==1 ? Drugs.ExpensiveStr1 :
                                             Drugs.ExpensiveStr2,
                                Drug[i].Name);
      } else if (Drug[i].Cheap) {
         g_string_append(text,Drug[i].CheapStr);
      } 
      First=FALSE;
   }
   if (!First) SendPrintMessage(NULL,C_NONE,To,text->str);
   g_string_truncate(text,0);
   for (i=0;i<NumDrug;i++) {
      g_string_sprintfa(text,"%s^",(prstr=pricetostr(To->Drugs[i].Price)));
      g_free(prstr);
   }
   SendServerMessage(NULL,C_NONE,C_DRUGHERE,To,text->str);
   g_string_free(text,TRUE);
}

void GenerateDrugsHere(Player *To,gchar *Deal) {
/* Generates drug prices and drug busts etc. for player "To" */
/* "Deal" is an array of chars of size NumDrug               */
   int NumEvents,NumDrugs,NumRandom,i;
   for (i=0;i<NumDrug;i++) {
      To->Drugs[i].Price=0;
      Deal[i]=0;
   }
   NumEvents=0;
   if (brandom(0,100)<70) NumEvents=1;
   if (brandom(0,100)<40 && NumEvents==1) NumEvents=2;
   if (brandom(0,100)<5 && NumEvents==2) NumEvents=3;
   NumDrugs=0;
   while (NumEvents>0) {
      i=brandom(0,NumDrug);
      if (Deal[i]!=0) continue;
      if (Drug[i].Expensive && (!Drug[i].Cheap || brandom(0,100)<50)) {
         Deal[i]=brandom(1,3);
         To->Drugs[i].Price=brandom(Drug[i].MinPrice,Drug[i].MaxPrice)
                            *Drugs.ExpensiveMultiply;
         NumDrugs++;
         NumEvents--;
      } else if (Drug[i].Cheap) {
         Deal[i]=1;
         To->Drugs[i].Price=brandom(Drug[i].MinPrice,Drug[i].MaxPrice)
                            /Drugs.CheapDivide;
         NumDrugs++;
         NumEvents--;
      }
   }
   NumRandom=brandom(Location[(int)To->IsAt].MinDrug,
                     Location[(int)To->IsAt].MaxDrug);
   if (NumRandom > NumDrug) NumRandom=NumDrug;

   NumDrugs=NumRandom-NumDrugs;
   while (NumDrugs>0) {
      i=brandom(0,NumDrug);
      if (To->Drugs[i].Price==0) {
         To->Drugs[i].Price=brandom(Drug[i].MinPrice,Drug[i].MaxPrice);
         NumDrugs--;
      }
   }
}

void HandleAnswer(Player *From,Player *To,char *answer) {
/* Handles the incoming message in "answer" from player "From" and */
/* intended for player "To".                                       */
   int i;
   gchar *text;
   if (!From || From->EventNum==E_NONE) return;
   if (answer[0]=='Y' && From->EventNum==E_OFFOBJECT && From->Bitches.Price
       && From->Bitches.Price>From->Cash) answer[0]='N';
   if (answer[0]=='Y') switch (From->EventNum) { 
      case E_OFFOBJECT:
         if (g_slist_find(FirstServer,(gpointer)From->OnBehalfOf)) {
            g_message(_("%s: offer was on behalf of %s"),GetPlayerName(From),
                   GetPlayerName(From->OnBehalfOf));
            if (From->Bitches.Price) {
               text=g_strdup_printf(_("%s has accepted your %s!"
                                    "^Use the G key to contact your spy."),
                                    GetPlayerName(From),Names.Bitch);
               From->OnBehalfOf->Flags |= SPYINGON;
               SendPlayerData(From->OnBehalfOf);
               SendPrintMessage(NULL,C_NONE,From->OnBehalfOf,text);
               g_free(text);
               i=GetListEntry(&(From->SpyList),From->OnBehalfOf);
               if (i>=0) From->SpyList.Data[i].Turns=0;
            }
         }
         if (From->Bitches.Price) {
            text=g_strdup_printf("bitch^0^1");
            BuyObject(From,text);
            g_free(text);
         } else {
            for (i=0;i<NumGun;i++) if (From->Guns[i].Price) {
               text=g_strdup_printf("gun^%d^1",i);
               BuyObject(From,text);
               g_free(text);
               break;
            }
         }
         From->OnBehalfOf=NULL;
         From->EventNum++; SendEvent(From);
         break;
      case E_LOANSHARK:
         SendServerMessage(NULL,C_NONE,C_LOANSHARK,From,NULL);
         break;
      case E_BANK:
         SendServerMessage(NULL,C_NONE,C_BANK,From,NULL);
         break;
      case E_GUNSHOP:
         for (i=0;i<NumGun;i++) From->Guns[i].Price=Gun[i].Price;
         SendServerMessage(NULL,C_NONE,C_GUNSHOP,From,NULL);
         break;
      case E_HIREBITCH:
         text=g_strdup_printf("bitch^0^1");
         BuyObject(From,text);
         g_free(text);
         From->EventNum++; SendEvent(From);
         break;
      case E_ROUGHPUB:
         From->EventNum++; SendEvent(From);
         break;
      case E_WEED:
         FinishGame(From,_("You hallucinated for three days on the wildest "
"trip you ever imagined!^Then you died because your brain disintegrated!")); 
         break;
      case E_COPS:
         FireAtHardass(From,F_RUN);
         break;
      case E_DOCTOR:
         if (From->Cash >= From->DocPrice) {
            From->Cash -= From->DocPrice;
            From->Health=100;
            SendPlayerData(From);
         }
         FinishFightWithHardass(From,NULL);
         break;
   } else if (From->EventNum==E_COPS && 
              (answer[0]=='F' || answer[0]=='R')) {
      FireAtHardass(From,answer[0]=='F' ? F_FIGHT : F_RUN);
   } else if (From->EventNum==E_ARRIVE) {
      if ((answer[0]=='A' || answer[0]=='T') && 
          g_slist_find(FirstServer,(gpointer)From->Attacked)) {
         if (From->Attacked->IsAt==From->IsAt) {
            if (answer[0]=='A') {
               if (From->Attacked->EventNum<E_MAX) {
                  From->Attacked->ResyncNum=From->Attacked->EventNum;
               }
               From->Attacked->Flags |= FIGHTING;
               SendPlayerData(From->Attacked);
               From->Flags |= FIGHTING;
               SendPlayerData(From);
               From->Attacked->EventNum=E_DEFEND;
               From->ResyncNum=E_ARRIVE+1;
               From->EventNum=E_ATTACK;
               AttackPlayer(From,From->Attacked,AT_FIRST | AT_SHOOT);
/*          } else if (answer[0]=='T') {
               From->Flags |= TRADING;
               SendPlayerData(From);
               SendServerMessage(NULL,C_NONE,C_TRADE,From,NULL);*/
            }
         } else {
            text=g_strdup_printf(_("Too late - %s has just left!"),
                                 GetPlayerName(From->Attacked));
            SendPrintMessage(NULL,C_NONE,From,text);
            g_free(text);
            From->EventNum++; SendEvent(From);
         }
      } else {
         From->EventNum++; SendEvent(From);
      }
   } else switch(From->EventNum) { 
      case E_ROUGHPUB:
         From->EventNum++;
         From->EventNum++; SendEvent(From);
         break;
      case E_COPS:
         FireAtHardass(From,F_STAND);
         break;
      case E_DOCTOR:
         FinishFightWithHardass(From,NULL);
         break;
      case E_HIREBITCH: case E_GUNSHOP: case E_BANK: case E_LOANSHARK:
      case E_OFFOBJECT: case E_WEED:
         if (g_slist_find(FirstServer,(gpointer)From->OnBehalfOf)) {
            g_message(_("%s: offer was on behalf of %s"),GetPlayerName(From),
                      GetPlayerName(From->OnBehalfOf));
            if (From->Bitches.Price && From->EventNum==E_OFFOBJECT) {
               text=g_strdup_printf(_("%s has rejected your %s!"),
                                    GetPlayerName(From),Names.Bitch);
               GainBitch(From->OnBehalfOf);
               SendPlayerData(From->OnBehalfOf);
               SendPrintMessage(NULL,C_NONE,From->OnBehalfOf,text);
               g_free(text);
               RemoveListPlayer(&(From->SpyList),From->OnBehalfOf);
            }   
         }
         From->EventNum++; SendEvent(From);
         break;
   }
}

void BreakoffCombat(Player *Attack,char LeftGame) {
/* Withdraws from player-player combat that player "Attack" is     */
/* currently involved in. "LeftGame" is TRUE if "Attack" has just  */
/* left the game, in which case no more messages are sent to this  */
/* player (just the other side of the fight is cleaned up)         */
   Player *Defend,*Play,*Victor;
   GSList *list;
   gchar *text;
   char FightDone;
   if (!g_slist_find(FirstServer,(gpointer)Attack)) {
      g_warning("Players involved in a fight are not valid!");
      return;
   }
   if (Attack->EventNum!=E_DEFEND && Attack->EventNum!=E_ATTACK &&
        Attack->EventNum!=E_FREEFORALL && Attack->EventNum!=E_WAITATTACK) {
      g_warning("Players in fight are not attack/defending!");
      return;
   }
   Victor=NULL;

   if (Attack->EventNum==E_DEFEND) {
      text=g_strdup_printf(_("%s has got away!"),GetPlayerName(Attack));
      for (list=FirstServer;list;list=g_slist_next(list)) {
         Play=(Player *)list->data;
         if (Play->Attacked==Attack && (Play->EventNum==E_ATTACK ||
             Play->EventNum==E_WAITATTACK || Play->EventNum==E_FREEFORALL)) {
            ClearFightTimeout(Play);
            Play->Attacked=NULL;
            Play->Flags &= ~FIGHTING;
            SendPlayerData(Play);
            if (Attack->Health!=0) {
               SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Play,text);
            }
            Victor=Play;
            Play->EventNum=Play->ResyncNum; SendEvent(Play);
         } 
      }
      g_free(text);
   } else {
      ClearFightTimeout(Attack);
      Victor=Attack;
      Defend=Attack->Attacked;
      if (!g_slist_find(FirstServer,(gpointer)Defend)) {
         g_warning("Players involved in a fight are not valid!");
         return;
      }
      if (Defend->EventNum!=E_DEFEND) {
         g_warning("Players in fight are not attack/defending!");
         return;
      }
 
      Attack->Attacked=NULL;
      FightDone=TRUE;
      for (list=FirstServer;list;list=g_slist_next(list)) {
         Play=(Player *)list->data;
         if (Play->Attacked==Defend && (Play->EventNum==E_ATTACK ||
             Play->EventNum==E_WAITATTACK || Play->EventNum==E_FREEFORALL)) { 
            FightDone=FALSE; 
            break; 
         }
      }
      if (Attack->Health>0) {
         text=g_strdup_printf(_("%s has run off!"),GetPlayerName(Attack));
         SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Defend,text);
         g_free(text);
      }
      if (FightDone) {
         Defend->Flags &= ~FIGHTING;
         SendPlayerData(Defend);
         Defend->EventNum=Defend->ResyncNum; SendEvent(Defend);
      }
   }
   if (!LeftGame) { 
      if (Attack->Health>0) SendPrintMessage(NULL,C_NONE,Attack,
         _("Coward! You successfully escaped from the fight."));
      Attack->Flags &= ~FIGHTING; SendPlayerData(Attack);
      Attack->EventNum=Attack->ResyncNum; SendEvent(Attack);
   }
}

void AttackPlayer(Player *Attack,Player *Defend,char AttackType) {
/* Processes a player-player attack from player "Attack" to player "Defend" */
/* AttackType is the type of attack, and may contain the following flags:-  */
/*  AT_FIRST: Set if this is the first attack                               */
/*  AT_SHOOT: Set if this is an 'active' attack - i.e. player "Attack" is   */
/*            actually shooting, not just "not running"                     */
   int i,j;
   Inventory *Guns,*Drugs;
   price_t Bounty;
   int Damage,MaxDamage;
   GString *DefendText,*AttackText;
   gchar *prstr,*ActionText,*ArmamentText;
 
   if (!g_slist_find(FirstServer,(gpointer)Attack) || 
       !g_slist_find(FirstServer,(gpointer)Defend)) {
      g_warning("Players involved in a fight are not valid!");
      return;
   }
   if (Attack->EventNum!=E_DEFEND && Attack->EventNum!=E_ATTACK &&
       Attack->EventNum!=E_FREEFORALL) {
      g_warning("%s is in wrong state (%d) to attack!",
                GetPlayerName(Attack),Attack->EventNum);
      return;
   }
   if ((Attack->EventNum==E_ATTACK || Attack->EventNum==E_FREEFORALL) && 
       Defend->EventNum!=E_DEFEND) {
      g_warning("%s is trying to attack %s, who is in wrong state (%d)!",
                GetPlayerName(Attack),GetPlayerName(Defend),Defend->EventNum);
      return;
   }
   if (Attack->EventNum==E_DEFEND && Defend->EventNum!=E_WAITATTACK &&
       Defend->EventNum!=E_FREEFORALL) {
      g_warning("%s is trying to defend against %s, who is in wrong \
state (%d)!",GetPlayerName(Attack),GetPlayerName(Defend),Defend->EventNum);
      return;
   }
   MaxDamage=0;
   Damage=0;
   AttackText=g_string_new(NULL);
   DefendText=g_string_new(NULL);
   for (i=0;i<NumGun;i++) {
      if (Gun[i].Damage>MaxDamage) MaxDamage=Gun[i].Damage;
      Damage+=Gun[i].Damage*Attack->Guns[i].Carried;
   }
   MaxDamage *= (Attack->Bitches.Carried+2);
   MaxDamage = Damage*100/MaxDamage;

   Guns=(Inventory *)g_malloc0(sizeof(Inventory)*NumGun);
   Drugs=(Inventory *)g_malloc0(sizeof(Inventory)*NumDrug);
   ClearInventory(Guns,Drugs);
   ArmamentText= MaxDamage<10 ? _("pitifully armed")       :
                 MaxDamage<25 ? _("lightly armed")         :
                 MaxDamage<60 ? _("moderately well armed") :
                 MaxDamage<80 ? _("heavily armed")         :
                                _("armed to the teeth");
   ActionText=AttackType&AT_SHOOT ? _(" fires and ") :
                                     _(" stands and takes it.");
   if (AttackType&AT_FIRST) {
      if (Attack->Bitches.Carried>0) {
         g_string_sprintf(DefendText,_("%s arrives, with %d %s, %s,^%s"),
                          GetPlayerName(Attack),Attack->Bitches.Carried,
                          Names.Bitches,ArmamentText,ActionText);
      } else {
         g_string_sprintf(DefendText,_("%s arrives, %s,^%s"),
                          GetPlayerName(Attack),ArmamentText,ActionText);
      }
   } else {
      if (AttackType&AT_SHOOT) {
         g_string_sprintf(DefendText,_("%s fires and "),GetPlayerName(Attack));
      } else {
         g_string_sprintf(DefendText,_("%s stands and takes it."),
                          GetPlayerName(Attack));
      }
   }
   Damage=0;
   if (AttackType&AT_SHOOT) {
      for (i=0;i<NumGun;i++) for (j=0;j<Attack->Guns[i].Carried;j++) {
         Damage+=brandom(0,Gun[i].Damage);
      }
   }
   if (Damage==0) {
      if (AttackType & AT_SHOOT) {
         g_string_append(DefendText,_("misses you!"));
         g_string_sprintf(AttackText,_("You failed to hit %s."),
                          GetPlayerName(Defend));
      } else {
         g_string_assign(AttackText,_("You stand and take it."));
      }
      SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Attack,AttackText->str);
   } else {
      g_string_append(DefendText,_("hits you, man!"));
      if (Damage>=Defend->Health) {
         if (Defend->Bitches.Carried==0) {
            g_string_append(DefendText,_(" You've been wasted! What a drag!"));
            g_string_sprintf(AttackText,_("You hit and killed %s"),
                             GetPlayerName(Defend));
            Defend->Health=0;
            Bounty=Defend->Cash+Defend->Bank-Defend->Debt;
            if (Bounty>0) Attack->Cash+=Bounty;
            SendPlayerData(Defend);
            SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Defend,DefendText->str);
            AddInventory(Guns,Defend->Guns,NumGun);
            AddInventory(Drugs,Defend->Drugs,NumDrug);
            TruncateInventoryFor(Guns,Drugs,Attack);
            if (!IsInventoryClear(Guns,Drugs)) {
               AddInventory(Attack->Guns,Guns,NumGun);
               AddInventory(Attack->Drugs,Drugs,NumDrug);
               ChangeSpaceForInventory(Guns,Drugs,Attack);
               SendPlayerData(Attack);
               g_string_append(AttackText,_(", and loot the body!"));
            } else g_string_append_c(AttackText,'!');
            SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Attack,AttackText->str);
            g_free(Guns); g_free(Drugs); 
            g_string_free(AttackText,TRUE);
            FinishGame(Defend,DefendText->str);
            g_string_free(DefendText,TRUE);
            return;
         } else {
            g_string_sprintfa(DefendText,_("^You lost a %s, man!"),
                                         Names.Bitch);
            Bounty=(Defend->Cash+Defend->Bank-Defend->Debt)*
                   Defend->Bitches.Carried/1000L;
            if (Bounty>0) {
               g_string_sprintf(AttackText,_("You are paid a bounty of %s in "
                                "reward for killing^one of %s's %s"),
                                prstr=FormatPrice(Bounty),
                                GetPlayerName(Defend),Names.Bitches);
               g_free(prstr);
               Attack->Cash+=Bounty;
               SendPlayerData(Attack);
            } else {
               g_string_sprintf(AttackText,_("You killed one of %s's %s "
                                "(%d left)"),GetPlayerName(Defend),
                                Names.Bitches,Defend->Bitches.Carried-1);
            }
            LoseBitch(Defend,Guns,Drugs);
            TruncateInventoryFor(Guns,Drugs,Attack);
            if (!IsInventoryClear(Guns,Drugs)) {
               AddInventory(Attack->Guns,Guns,NumGun);
               AddInventory(Attack->Drugs,Drugs,NumDrug);
               ChangeSpaceForInventory(Guns,Drugs,Attack);
               SendPlayerData(Attack);
               g_string_append(AttackText,_(", and loot the body!"));
            } else g_string_append_c(AttackText,'!');
            SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Attack,AttackText->str);
            Defend->Health=100;
         }
      } else {
         Defend->Health-=Damage;
         g_string_sprintf(AttackText,_("You fire, and hit %s!"),
                          GetPlayerName(Defend));
         SendServerMessage(NULL,C_NONE,C_FIGHTPRINT,Attack,AttackText->str);
      }
   }
   if (Attack->EventNum==E_ATTACK || Attack->EventNum==E_FREEFORALL) {
      Attack->EventNum=E_WAITATTACK; SetFightTimeout(Attack);
   } else if (Attack->EventNum==E_DEFEND) {
      Defend->EventNum=E_ATTACK; SetFightTimeout(Defend);
   }
   SendPlayerData(Defend);
   SendServerMessage(Attack,C_NONE,C_FIGHTPRINT,Defend,DefendText->str);
   g_string_free(AttackText,TRUE);
   g_string_free(DefendText,TRUE);
   g_free(Guns); g_free(Drugs);
}

void BuyObject(Player *From,char *data) {
/* Processes a request stored in "data" from player "From" to buy an  */
/* object (bitch, gun, or drug)                                       */
/* Objects can be sold if the amount given in "data" is negative, and */
/* given away if their current price is zero.                         */
   char *cp,*type;
   gchar *lone,*deputy;
   int index,i,amount;
   cp=data;
   type=GetNextWord(&cp,"");
   index=GetNextInt(&cp,0);
   amount=GetNextInt(&cp,0);
   if (strcmp(type,"drug")==0) {
      if (index>=0 && index<NumDrug && From->Drugs[index].Carried+amount >= 0
          && From->CoatSize-amount >= 0 && (From->Drugs[index].Price!=0 || 
          amount<0) && From->Cash >= amount*From->Drugs[index].Price) {
         From->Drugs[index].Carried+=amount;
         From->CoatSize-=amount;
         From->Cash-=amount*From->Drugs[index].Price;
         SendPlayerData(From); 

         if (!Sanitized && (From->Drugs[index].Price==0 && brandom(0,100)<Cops.DropProb)) {
            lone=g_strdup_printf(_("YN^Officer %%s spots you dropping %s, and "
                                 "chases you!"),Names.Drugs);
            deputy=g_strdup_printf(_("YN^Officer %%s and %%d of his deputies "
                                   "spot you dropping %s, and chase you!"),
                                   Names.Drugs);
            StartOfficerHardass(From,From->EventNum,lone,deputy);
            g_free(lone); g_free(deputy);
         }
      }
   } else if (strcmp(type,"gun")==0) {
      if (index>=0 && index<NumGun && TotalGunsCarried(From)+amount >= 0
          && TotalGunsCarried(From)+amount <= From->Bitches.Carried+2
          && From->Guns[index].Price!=0
          && From->CoatSize-amount*Gun[index].Space >= 0
          && From->Cash >= amount*From->Guns[index].Price) {
         From->Guns[index].Carried+=amount;
         From->CoatSize-=amount*Gun[index].Space;
         From->Cash-=amount*From->Guns[index].Price;
         SendPlayerData(From);
      }
   } else if (strcmp(type,"bitch")==0) {
      if (From->Bitches.Carried+amount >= 0 
          && From->Bitches.Price!=0 
          && amount*From->Bitches.Price <= From->Cash) {
         for (i=0;i<amount;i++) GainBitch(From);
         if (amount>0) From->Cash-=amount*From->Bitches.Price;
         SendPlayerData(From);
      }
   } 
}

void ClearPrices(Player *Play) {
/* Clears the bitch and gun prices stored for player "Play" */
   int i;
   Play->Bitches.Price=0;
   for (i=0;i<NumGun;i++) Play->Guns[i].Price=0;
}

void GainBitch(Player *Play) {
/* Gives player "Play" a new bitch (or larger trenchcoat) */
   Play->CoatSize+=10;
   Play->Bitches.Carried++;
}

int LoseBitch(Player *Play,Inventory *Guns,Inventory *Drugs) {
/* Loses one bitch belonging to player "Play". If drugs or guns are */
/* lost with the bitch, 1 is returned (0 otherwise) and the lost    */
/* items are added to "Guns" and "Drugs" if non-NULL                */
   int losedrug=0,i,num,drugslost;
   int *GunIndex,tmp;
   GunIndex=g_new(int,NumGun);
   ClearInventory(Guns,Drugs);
   Play->CoatSize-=10;
   if (TotalGunsCarried(Play)>0) {
      if (brandom(0,100)<TotalGunsCarried(Play)*100/(Play->Bitches.Carried+2)) {
         for (i=0;i<NumGun;i++) GunIndex[i]=i;
         for (i=0;i<NumGun*5;i++) {
            num=brandom(0,NumGun-1);
            tmp=GunIndex[num+1];
            GunIndex[num+1]=GunIndex[num];
            GunIndex[num]=tmp;
         }
         for (i=0;i<NumGun;i++) {
            if (Play->Guns[GunIndex[i]].Carried > 0) {
               Play->Guns[GunIndex[i]].Carried--; losedrug=1;
               Play->CoatSize+=Gun[GunIndex[i]].Space;
               if (Guns) Guns[GunIndex[i]].Carried++;
               break;
            }
         }
      }
   }
   for (i=0;i<NumDrug;i++) if (Play->Drugs[i].Carried>0) {
      num=(int)((float)Play->Drugs[i].Carried/(Play->Bitches.Carried+2.0)+0.5);
      if (num>0) {
         Play->Drugs[i].Carried-=num;
         if (Drugs) Drugs[i].Carried+=num;
         Play->CoatSize+=num;
         losedrug=1;
      }
   }
   while (Play->CoatSize<0) {
      drugslost=0;
      for (i=0;i<NumDrug;i++) {
         if (Play->Drugs[i].Carried>0) {
            losedrug=1; drugslost=1;
            Play->Drugs[i].Carried--;
            Play->CoatSize++;
            if (Play->CoatSize>=0) break;
         }
      }
      if (!drugslost) for (i=0;i<NumGun;i++) {
         if (Play->Guns[i].Carried>0) {
            losedrug=1;
            Play->Guns[i].Carried--;
            Play->CoatSize+=Gun[i].Space;
            if (Play->CoatSize>=0) break;
         }
      }
   }
   Play->Bitches.Carried--;
   g_free(GunIndex);
   return losedrug;
}

void SetFightTimeout(Player *Play) {
   if (FightTimeout) Play->FightTimeout=time(NULL)+(time_t)FightTimeout;
   else Play->FightTimeout=0;
}

void ClearFightTimeout(Player *Play) {
   Play->FightTimeout=0;
}

int AddTimeout(time_t timeout,time_t timenow,int *mintime) {
   if (timeout==0) return 0;
   else if (timeout<=timenow) return 1;
   else {
      if (*mintime<0 || timeout-timenow < *mintime) *mintime=timeout-timenow;
      return 0;
   }
}

int GetMinimumTimeout(GSList *First) {
   Player *Play;
   GSList *list;
   int mintime=-1;
   time_t timenow;

   timenow=time(NULL);
   if (AddTimeout(MetaTimeout,timenow,&mintime)) return 0;
   for (list=First;list;list=g_slist_next(list)) {
      Play=(Player *)list->data;
      if (AddTimeout(Play->FightTimeout,timenow,&mintime) ||
          AddTimeout(Play->IdleTimeout,timenow,&mintime) ||
          AddTimeout(Play->ConnectTimeout,timenow,&mintime)) return 0;
   }
   return mintime;
}

GSList *HandleTimeouts(GSList *First) {
   GSList *list,*nextlist;
   Player *Play;
   gchar *text;
   time_t timenow;

   timenow=time(NULL);
   if (MetaTimeout!=0 && MetaTimeout<=timenow) {
      MetaTimeout=0;
      RegisterWithMetaServer(TRUE,FALSE);
   }
   list=First;
   while (list) {
      nextlist=g_slist_next(list);
      Play=(Player *)list->data;
      if (Play->IdleTimeout!=0 && Play->IdleTimeout<=timenow) {
         Play->IdleTimeout=0;
         g_message(_("Player removed due to idle timeout"));
         SendPrintMessage(NULL,C_NONE,Play,"Disconnected due to idle timeout");
         ClientLeftServer(Play);
/*       shutdown(Play->fd,SD_RECV);*/
/* Make sure they do actually disconnect, eventually! */
         if (ConnectTimeout) {
            Play->ConnectTimeout=time(NULL)+(time_t)ConnectTimeout;
         }
      } else if (Play->ConnectTimeout!=0 && Play->ConnectTimeout<=timenow) {
         Play->ConnectTimeout=0;
         g_message(_("Player removed due to connect timeout"));
         First=RemovePlayer(Play,First);
      } else if (Play->FightTimeout!=0 && Play->FightTimeout<=timenow) {
         ClearFightTimeout(Play);
         if (Play->EventNum==E_WAITATTACK) {
            Play->EventNum=E_FREEFORALL;
            text=g_strdup_printf(_("%s fails to return fire..."),
                                 GetPlayerName(Play->Attacked));
            SendServerMessage(Play->Attacked,C_NONE,C_FIGHTPRINT,Play,text);
            g_free(text);
         } else if (Play->EventNum==E_ATTACK) {
            Play->EventNum=E_FREEFORALL;
            text=g_strdup_printf(_("%s fails to return fire..."),
                                 GetPlayerName(Play));
            SendServerMessage(Play,C_NONE,C_FIGHTPRINT,Play->Attacked,text);
            g_free(text);
         }
      }
      list=nextlist;
   }
   return First;
}
