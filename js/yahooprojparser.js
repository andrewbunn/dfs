var str = "";
getData();
function delayGetData()
{
	setTimeout(getData, 2000);
}
function getData()
{
	var table = document.getElementsByClassName("players")[0].getElementsByTagName("tbody")[0];
	var players = table.getElementsByTagName("tr");
	for (var i =0; i < players.length; i++)
	{
		var row = players[i];
		var cols = row.getElementsByTagName("td");
		var name = cols[1].getElementsByClassName("ysf-player-name")[0].innerText;
		name = name.split("-")[0].trim();
		name = name.substring(0, name.lastIndexOf(' '));
		str += name.trim();
		var score = cols[6].innerText;
		str += "," + score.trim();
		var allZero = true;
		for (var j = 10; j < cols.length; j++)
		{
			var val = cols[j].innerText;
			str += "," + val.trim();
			if (parseInt(cols[j].innerText) != 0)
			{
				allZero = false;
			}
		}
		str += "\n";
		if (allZero)
		{
			break;
		}
	}
	
	var navlist = document.getElementsByClassName("pagingnavlist")[0];
	var next = navlist.getElementsByClassName("last")[0].getElementsByTagName("a")[0];
	next.onclick = delayGetData;
	
	window.location.href = 'data:text/csv;charset=utf-8,' + encodeURIComponent(str);
}
