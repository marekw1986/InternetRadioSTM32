function drawMenu() {
    var menu = document.getElementById("horizontal_menu").innerHTML;
    menu = '<ul class="menu_poziome">';
    menu += '<li><a href="/ui/index.htm">Start</a></li>';
    menu += '<li><a href="/ui/config.htm">Konfiguracja</a></li>';
    menu += '<li><a href="/ui/pass.htm">DostÄ™p</a></li>';
    menu += '<li><a href="/ui/status.htm">Status</a></li>';
    menu += '</ul>';
}
function reset(hard) {
    if (hard) {
        r = confirm("Przywróci? domy?lne ustawienia?");
        if (!r) {return false;}
    }
    var token = document.getElementById("token");
	var result = document.getElementById("result");
	var xmlhttp;
	try {
	   xmlhttp = new XMLHttpRequest();
	}
	catch (e) {
	   try {
		  xmlhttp = new ActiveXObject("Msxml2.XMLHTTP");	
	   }
	   catch (e) {
			try {
				xmlhttp = new ActiveXObject("Microsoft.XMLHTTP");
			}
			catch (e){
				result.innerHTML = "Wyst?pi? b??d przegl?darki, uniemo?liwiaj?cy wykonanie zdalnego resetu urz?dzenia.";
				return false;
			}	
		}
	}
    var url = "/ui/reset.cgi?token=" + token.value;
    if (hard) {url += "&hardreset=yes";}
	xmlhttp.open("GET", url, true);
	xmlhttp.send();
	result.innerHTML="Wykonuj? reset urz?dzenia.";
	setTimeout(function(){result.innerHTML+="<br>Przekierowuj? na: http://~newip~/";window.location.replace("http://~newip~/?_=" + (new Date()).getTime());}, 3000);
	return true;
}