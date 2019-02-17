var rp = 0;
var gp = 0;
var bp = 0;

var sliders = document.querySelectorAll(".slider");

sliders.forEach( function(slider){
	slider.oninput = function(){
		var divStr = '#'+this.id+'t';
		var div = document.querySelector(divStr);
		div.innerHTML = this.value.toString();
		var colorStr = (parseInt(sliders[0].value)/1000*255).toFixed() +',';
		colorStr += (parseInt(sliders[1].value)/1000*255).toFixed() +',';
		colorStr += (parseInt(sliders[2].value)/1000*255).toFixed();
		document.querySelector('#color_chart').style.backgroundColor = 'rgb(' + colorStr + ')';
}});


function checkForUpdate(){
	var r = Math.round(parseInt(document.querySelector('#r').value));
	var g = Math.round(parseInt(document.querySelector('#g').value));
	var b = Math.round(parseInt(document.querySelector('#b').value));
	r = Math.round(Math.pow(r/1000,3)*1000);
	g = Math.round(Math.pow(g/1000,3)*1000);
	b = Math.round(Math.pow(b/1000,3)*1000);
	var newVal = false; 
	if(rp != r){newVal = true}
	if(gp != g){newVal = true}
	if(bp != b){newVal = true}
	if(newVal){
		console.log('Updating RGB Val');
		updateRGB(r,g,b);	
	}
	rp = r; gp = g; bp = b;
}

var lockout = false;

function updateRGB(r,g,b) {
	if(!lockout ){
		var xhttp = new XMLHttpRequest();
		xhttp.onreadystatechange = function() {
			    if (this.readyState == 4 && this.status == 200) {
     				lockout = false;
    			}else{
    				
    			}
		};
		var sendStr = "rgb=" + r + '_' + g + '_' + b;
		xhttp.open("GET", sendStr, true);
		lockout = true;
		xhttp.send();
			
	}


}

setInterval(checkForUpdate,100);