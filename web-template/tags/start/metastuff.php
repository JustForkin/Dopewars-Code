<?php

  include "include/dopefunc.php";

/* This is the main functionality of the dopewars metaserver. It should be
   included from metaserver.php, and called as MainFunc(). */
  function MainFunc($dbhand) {
    global $getlist,$server,$output,$textoutput,$uplink;
    if (!$dbhand) {
      FatalError("Could not connect to dopewars database server");
    }
    if (!@mysql_select_db("dopewars",$dbhand)) {
      FatalError("Could not locate the main dopewars database!");
    }
    if ($output=='text') {
      header("Content-type: text/plain");
      $textoutput=TRUE;
    } else $textoutput=FALSE;
    $servername='';
    if ($getlist) {
      ShowServers($dbhand);
/*  } else if ($uplink) {
      DoUplink($dbhand);*/
    } else if ($server) {
      ServerInfo($dbhand,$server);
      $servername=$server;
    } else {
      RegisterServer($dbhand);
    }
    PrintHTMLFooter($servername);
  }

/* Maximum number of high scores */
  $NUMHISCORES = 18;

  function ShowServers($dbhand) {
    global $output,$getlist,$DOCROOT,$mirrorID;
    global $HTTP_SERVER_VARS;
    PrintHTMLHeader("Active dopewars servers",TRUE);

/* First, wipe any servers that haven't reported in for 4 hours
   (14400 seconds) and any associated tables */
    $result = dope_query("SELECT ID FROM servers WHERE UNIX_TIMESTAMP(LastUpdate)+14400 < UNIX_TIMESTAMP(NOW())");
    while ($row=mysql_fetch_array($result)) {
      dope_query("DELETE FROM highscores WHERE ServerID='".$row['ID']."'");
    }
    $result = dope_query("DELETE FROM servers WHERE UNIX_TIMESTAMP(LastUpdate)+14400 < UNIX_TIMESTAMP(NOW())");

    $result = dope_query("SELECT servers.*,COUNT(Score) AS NumScores FROM servers LEFT JOIN highscores ON ServerID=servers.ID GROUP BY servers.ID ORDER BY UpSince");
    if ($output=='text') {
      print "MetaServer:\n";
      while ($row=mysql_fetch_array($result)) {
        print $row['HostName']."\n".$row['Port']."\n".$row['Version']."\n";
        if ($getlist>=2) {
          print $row['Players']."\n";
        }
        print $row['MaxPlayers']."\n";
        print FormatTimestamp($row['LastUpdate'])."\n".$row['Comment']."\n";
        if ($getlist>=2) {
          print FormatTimestamp($row['UpSince'])."\n";
        }
      }
    } else {
?>
<p>dopewars incorporates limited multiplayer capabilities, with a server
mode (the -s switch). The list below is maintained automatically, providing
that you're running a server of version 1.5.1 or above. In some cases (usually
if you're connecting via. a proxy) the metaserver may report your domain
name incorrectly, or refuse connection entirely; see the
<?php print "<a href=\"".$DOCROOT."docs/metaserver.html\">metaserver</a>\n"; ?>
page for tips on fixing this problem. Additional problems can usually be solved
by <a href="mailto:ben@bellatrix.pcl.ox.ac.uk">emailing</a> the metaserver
maintainer.</p>

<p>To prevent your server announcing itself to the outside world, add the
line "MetaServer.Active=0" (without the quotes) to your dopewars configuration
file (/etc/dopewars or ~/.dopewars) or run the server with the <b>-S</b>
command line option rather than <b>-s</b>.</p>

<p>Please note that all servers are checked for service; those that are
unreachable or refuse connection will be removed from this list.</p>

<p>If a given server is reporting its high scores to the metaserver, the
server name will be hyperlinked (only versions 1.5.1 and above do this - older
versions only work with the "old" metaserver). Follow the link to see the 
current high scores.</p>

<?php
/*    print "<p>refer: ".$HTTP_SERVER_VARS['HTTP_REFERER']."</p>\n";*/
      print "<table border=\"1\">\n\n";
      print "<tr><th>Server name</th><th>Port</th><th>Version</th>\n".
            "<th>Players</th><th>Max. Players</th><th>Last update</th>\n".
            "<th>Up since</th><th>Comment</th></tr>\n\n";
      while ($row=mysql_fetch_array($result)) {
        print "<tr>";
        print "<td>";
        HTMLQuote($row['Version']);
        HTMLQuote($row['Comment']);
        if ($row['NumScores']>0) {
          print "<a href=\"metaserver.php?server=".$row['HostName'].
                $mirrorID."\">";
        }
        print $row['HostName'];
        if ($row['NumScores']>0) print "</a>";
        print "</td>\n";
        print "<td>".$row['Port']."</td>\n";
        print "<td>".$row['Version']."</td>\n";
        print "<td>".$row['Players']."</td>\n";
        print "<td>".$row['MaxPlayers']."</td>\n";
        print "<td>".FormatTimestamp($row['LastUpdate'])."</td>\n";
        print "<td>".FormatTimestamp($row['UpSince'])."</td>\n";
        print "<td>".$row['Comment']."</td>\n";
        print "</tr>\n\n";
      }
      print "</table>\n\n";
    }
  }

/* Quote all HTML stuff in $str, and modify it in place. */
  function HTMLQuote(&$str) {
    $str = htmlspecialchars($str,ENT_QUOTES);
  }

  function FormatTimestamp($timestamp) {
    $year=substr($timestamp,0,4);
    $month=substr($timestamp,4,2);
    $day=substr($timestamp,6,2);
    $hour=substr($timestamp,8,2);
    $minute=substr($timestamp,10,2);
    return "$hour:$minute on $day/$month/$year";
  }

  function dope_query($query) {
    $result = mysql_query($query);
    if (!$result) {
      FatalError("performing query: ".mysql_error());
    } else return $result;
  }

  function ValidateServerDetails() {
    global $port,$nm,$dt,$sc,$NUMHISCORES,$version,$comment;
    global $players,$maxplay,$hostname,$password;
    $port=(int)$port;
    $players=(int)$players;
    $maxplay=(int)$maxplay;
    if (!$port) { FatalError("Invalid server port supplied"); }

/* Convert dates into SQL-friendly format */
    for ($i=0;$i<$NUMHISCORES;$i++) if ($nm[$i]) {
      if (!$dt[$i] || strlen($dt[$i])!=10) {
        FatalError("Invalid high score date ".$dt[$i]);
      }
      $day=(int)substr($dt[$i],0,2);
      $month=(int)substr($dt[$i],3,2);
      $year=(int)substr($dt[$i],6,4);
      $dt[$i] = sprintf("%04d-%02d-%02d",$year,$month,$day);
      MyAddSlashes($nm[$i]);
      MyAddSlashes($sc[$i]);
    }

/* Quote text strings (if necessary) for SQL */
    MyAddSlashes($version);
    MyAddSlashes($comment);
    MyAddSlashes($hostname);
    MyAddSlashes($password);
  }

/* Escape quote characters etc. if magic quotes are _not_ being used */
  function MyAddSlashes(&$str) {
    if (!get_magic_quotes_gpc()) $str=addslashes($str);
  }

  function FatalError($msg) {
    PrintParagraph("FATAL ERROR: $msg"); exit();
  }

  function PrintParagraph($msg) {
    global $textoutput;
    if ($textoutput) print "$msg\n\n";
    else print "<p>$msg</p>\n\n";
  }

  function CheckHostOverride(&$realhostname) {
    global $password,$hostname;
    if ($password && $hostname) {
      $result = dope_query("SELECT * FROM hostoverride WHERE Password='$password' AND HostName='$hostname'");
      if (!(mysql_affected_rows())) {
        FatalError("Password and hostname do not match!");
      }
      $realhostname = $hostname;
      PrintParagraph("Hostname override password accepted");
      return TRUE;
    }
    return FALSE;
  }

  function CheckHostNotOverridden($hostname) {
    $result = dope_query("SELECT * FROM hostoverride WHERE HostName='$hostname'");
    if (mysql_affected_rows()) {
      FatalError("Host $hostname is password-protected - not updating server details");
    }
  }

  function GetServerLocation(&$realhostname,&$remoteIP,&$proxyIP) {
    global $HTTP_SERVER_VARS;
    global $password,$hostname;

    $remoteIP = $HTTP_SERVER_VARS['REMOTE_ADDR'];
    $proxyIP = '';

    if (CheckHostOverride($realhostname)) return;

    if ($HTTP_SERVER_VARS['HTTP_X_FORWARDED_FOR'] != '') {
      $fwdIPs = $HTTP_SERVER_VARS['HTTP_X_FORWARDED_FOR'];
      $proxyIP = $HTTP_SERVER_VARS['REMOTE_ADDR'];
/* Check for multiple forwards, and take the first IP if necessary */
      $splitIPs = explode(", ",$fwdIPs);
      if ($splitIPs==$fwdIPs) $remoteIP=$fwdIPs;
      else $remoteIP=$splitIPs[0];
    }

    $result = dope_query("SELECT HostName FROM localdns WHERE IP='$remoteIP'");
    $row=mysql_fetch_array($result);
    if ($row) $realhostname = $row['HostName'];

    if (!$realhostname) $realhostname=@gethostbyaddr($remoteIP);

    CheckHostNotOverridden($realhostname);
  }

  function CheckServerConnect($HostName,$Port) {
/*  $fp = fsockopen($HostName,$Port,$errno,$errstr,60);
    if (!$fp) {
      FatalError("Could not connect to server at $HostName:$Port ($errstr). NOT registering server - please check that the hostname and port are correct and that your firewall is not blocking connections, and try again later.");
    }*/
  }

  function CreateCredential() {
    return "dummy";
  }

  function CheckCredential($ProperCred) {
    global $credential;
/*  if ($credential != $ProperCred) {
      FatalError("Credential mismatch: server details not updated");
    }*/
  }

  function ValidDynamicDNS() {
    global $password,$hostname;
    if ($hostname || !$password) return FALSE;
    $result = dope_query("SELECT * FROM dynamicdns WHERE Password='$password'");
    return (mysql_affected_rows()!=0);
  }

  function CheckValidProxy($oldproxyIP,$proxyIP,$HostName) {
    if ($proxyIP == $oldproxyIP) return;
    if (!$oldproxyIP) {
      FatalError("Cannot update $HostName - you are connecting via. proxy $proxyIP, and this host last connected directly");
    } else if (!proxyIP) {
      FatalError("Cannot update $HostName - you are connecting directly, and this host last connected via. a proxy");
    } else {
/*    FatalError("Cannot update $HostName - you are connecting via. a different proxy to the one it last connected via."); */

/* We'll let this go for now - the paranoid will just have to use the password
   authentication scheme instead */
      return;
    }
  }

  function RegisterServer($dbhand) {
    global $port,$version,$comment,$players,$maxplay,$up,$password;
    global $nm,$dt,$st,$sc,$NUMHISCORES;

    PrintHTMLHeader("dopewars server registration");

    GetServerLocation($HostName,$remoteIP,$proxyIP);
    ValidateServerDetails();

    $validdyn=ValidDynamicDNS();

    if ($validdyn) {
      PrintParagraph("Dynamic DNS password accepted");
      $result = dope_query("SELECT ID,Credential,ProxyIP FROM servers,dynamicdns WHERE ServerID=ID AND Password='$password'");
    } else {
      $result = dope_query("SELECT ID,Credential,ProxyIP FROM servers WHERE HostName='$HostName' AND Port='$port'");
    }
    $createnew=(mysql_affected_rows()==0);

    if (!$createnew) {
      $row=mysql_fetch_array($result);
      $serverID=$row['ID'];
      $credential=$row['Credential'];
      $oldproxyIP=$row['ProxyIP'];
      CheckValidProxy($oldproxyIP,$proxyIP,$HostName);
    }

    PrintParagraph("You are apparently at $HostName:$port ($remoteIP)");
    PrintParagraph("Server is: ".($up ? 'up' : 'down'));

    if ($up==0) {
      if (!$createnew) {
        CheckCredential($credential);
        dope_query("DELETE FROM servers WHERE ID='$serverID'");
        dope_query("DELETE FROM highscores WHERE ServerID='$serverID'");
        if ($validdyn) dope_query("UPDATE dynamicdns SET ServerID=NULL WHERE Password='$password'");
      }
      return;
    }

    if ($createnew) {
      CheckServerConnect($HostName,$port);
      $credential=CreateCredential();
/*    PrintParagraph("Credential: $credential");*/
      $query="INSERT INTO servers SET UpSince=NOW(), Credential='$credential', ";
    } else {
      CheckCredential($credential);
      $query="UPDATE servers SET ";
    }

    $query .= "HostName='$HostName', ";
    $query .= "Port='$port', ";
    $query .= "LastUpdate=NOW(), ";
    $query .= "Version='$version', ";
    $query .= "Players='$players', ";
    $query .= "MaxPlayers='$maxplay', ";
    $query .= "IP='$remoteIP', ";
    $query .= "ProxyIP='$proxyIP', ";
    $query .= "Comment='$comment' ";

    if (!$createnew) $query .= "WHERE ID='$serverID'";

    $result = dope_query($query);
    if ($createnew) {
      $serverID=mysql_insert_id();
      if ($validdyn) dope_query("UPDATE dynamicdns SET ServerID='$serverID' WHERE Password='$password'");
    }

    if (sizeof($nm)>0) {
      $result = dope_query("DELETE FROM highscores WHERE ServerID='$serverID'");
    }

    for ($i=0;$i<$NUMHISCORES;$i++) if ($nm[$i]) {
      $result = dope_query("INSERT INTO highscores SET Name='$nm[$i]', Date='
$dt[$i]', Status='$st[$i]', Score='$sc[$i]', ServerID='$serverID', ID='$i'");
    }
  }

  function ServerInfo($dbhand,$server) {
    PrintHTMLHeader("dopewars server $server");
    $result = dope_query("SELECT Score, Date, Name, Status FROM servers,highscores WHERE serverID=servers.ID AND HostName='$server' ORDER BY highscores.ID");

?>

<h3>Scores for multiplayer mode</h3>

<?php
    if (!(mysql_affected_rows())) {
      print "<p>No data available for this server - it has, most likely, gone down.</p>\n\n";
      return;
    }

    print "<table border=\"1\"><tr><th>Score</th><th>Date</th>\n";
    print "<th>Name</th><th>Status</th></tr>\n\n";
    while (($row=mysql_fetch_array($result))) {
      HTMLQuote($row['Score']);
      HTMLQuote($row['Name']);
      print "<tr>\n";
      print "<td>".$row['Score']."</td>\n";
      print "<td>".$row['Date']."</td>\n";
      print "<td>".$row['Name']."</td>\n";
      print "<td>".$row['Status']."</td>\n";
      print "</tr>\n";
    }
    print "</table>\n";
  }

  function PrintHTMLHeader($title,$ismain=FALSE) {
    global $textoutput;
    if ($textoutput) return;

    StartHTML($title,$ismain ? "Active servers" : "");
  }

  function PrintHTMLFooter($servername) {
    global $textoutput;
    if ($textoutput) return;

    if ($servername) WriteNavLinks("Active servers",$servername);
    else WriteNavLinks("Active servers");

    EndHTML("metastuff.php");
  }

  function DoUplink($dbhand) {
    global $uplink,$serv,$port,$vers,$play,$maxp,$upda,$comm,$upsi;
    global $HTTP_SERVER_VARS;

    $result = dope_query("SELECT IP from uplink WHERE Password='$uplink'");

    if ($HTTP_SERVER_VARS['HTTP_X_FORWARDED_FOR'] != '') {
      $fwdIPs = $HTTP_SERVER_VARS['HTTP_X_FORWARDED_FOR'];
      $splitIPs = explode(", ",$fwdIPs);
      if ($splitIPs==$fwdIPs) $remoteIP=$fwdIPs;
      else $remoteIP=$splitIPs[0];
    } else {
      $remoteIP = $HTTP_SERVER_VARS['REMOTE_ADDR'];
    }
    if (($row=mysql_fetch_array($result)) && $row['IP']==$remoteIP) {
      PrintParagraph("Uplink request approved");
    } else {
      FatalError("Uplink request denied IP");
    }

    for ($i=0;$i<sizeof($serv);$i++) {
      $result = dope_query("SELECT ID,UNIX_TIMESTAMP(LastUpdate) AS Upd FROM servers WHERE HostName='$serv[$i]' AND Port='$port[$i]'");
      $createnew=(mysql_affected_rows()==0);
      $row=mysql_fetch_array($result);
      if ($createnew) {
        $query="INSERT INTO servers SET ";
      } else {
        $query="UPDATE servers SET ";
      }

/* Only update existing entries if the "new" entry has a more recent
   update time */
      if ($createnew || $row['Upd'] < $upda[$i]) {
        $query .= "HostName='$serv[$i]', ";
        $query .= "Port='$port[$i]', ";
        $query .= "LastUpdate=FROM_UNIXTIME($upda[$i]), ";
        $query .= "UpSince=FROM_UNIXTIME($upsi[$i]), ";
        $query .= "Version='$vers[$i]', ";
        $query .= "Players='$play[$i]', ";
        $query .= "MaxPlayers='$maxp[$i]', ";
        $query .= "Comment='$comm[$i]'";
        if (!$createnew) $query .= " WHERE HostName='$serv[$i]' AND Port='$port[$i]'";
        dope_query($query);
      }
    }
  }
    
?>
