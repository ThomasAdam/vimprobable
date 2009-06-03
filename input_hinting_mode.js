/*
    (c) 2009 by Leon Winter, see LICENSE file
*/

function clearfocus() {
    if(document.activeElement && document.activeElement.blur) 
        document.activeElement.blur();
} 

function v(e, y) { 
    t = e.nodeName.toLowerCase(); 
    if((t == 'input' && /^(text|password)$/.test(e.type))
    || /^(select|textarea)$/.test(t)
    || e.contentEditable == 'true') 
        console.log('insertmode_'+(y=='focus'?'on':'off')); 
} 

if(document.activeElement) 
    v(document.activeElement,'focus'); 

m=['focus','blur']; 

for(i in m) 
    document.getElementsByTagName('body')[0].addEventListener(m[i], function(x) { 
        v(x.target,x.type); 
    }, true); 

document.getElementsByTagName("body")[0].appendChild(document.createElement("style")); 
document.styleSheets[0].addRule('.hinting_mode_hint', 'color: #000; background: #ff0');
document.styleSheets[0].addRule('.hinting_mode_hint_focus', 'color: #000; background: #8f0');

self.onunload = function() {
    v(document.activeElement, '');
};

function show_hints() { 
    var height = window.innerHeight; 
    var width = window.innerWidth; 
    var scrollX = document.defaultView.scrollX; 
    var scrollY = document.defaultView.scrollY; 
    var hinttags = "//html:*[@onclick or @onmouseover or @onmousedown or @onmouseup or @oncommand or @class='lk' or @role='link'] | //html:input[not(@type='hidden')] | //html:a | //html:area | //html:iframe | //html:textarea | //html:button | //html:select"; 
    var r = document.evaluate(hinttags, document,
        function(p) {
            return 'http://www.w3.org/1999/xhtml';
        }, XPathResult.ORDERED_NODE_ITERATOR_TYPE, null); 
    div = document.createElement("div"); 
    i = 1; 
    a = []; 
    while(elem = r.iterateNext()) 
    { 
        rect = elem.getBoundingClientRect(); 
        if (!rect || rect.top > height || rect.bottom < 0 || rect.left > width || rect.right < 0 || !(elem.getClientRects()[0])) 
            continue; 
        var computedStyle = document.defaultView.getComputedStyle(elem, null); 
        if (computedStyle.getPropertyValue("visibility") != "visible" || computedStyle.getPropertyValue("display") == "none") 
            continue; 
        var leftpos = Math.max((rect.left + scrollX), scrollX); 
        var toppos = Math.max((rect.top + scrollY), scrollY); 
        a.push(elem); 
        div.innerHTML += '<span id="hint' + i + '" style="position: absolute; top: ' + toppos + 'px; left: ' + leftpos + 'px; background: red; color: #fff; font: bold 10px monospace; z-index: 99">' + (i++) + '</span>';
    } 
    for(e in a) 
        a[e].className += " hinting_mode_hint"; 
    document.getElementsByTagName("body")[0].appendChild(div); 
    clearfocus(); 
    s = ""; 
    h = null; 
    window.onkeyup = function(e) 
    { 
        if(e.which == 13 && s != "") { 
            if(h) h.className = h.className.replace("_focus",""); 
            fire(parseInt(s)); 
        } 
        key = String.fromCharCode(e.which); 
        if (isNaN(parseInt(key))) 
            return; 
        s += key; 
        n = parseInt(s); 
        if(h != null) 
            h.className = h.className.replace("_focus",""); 
        if (i - 1 < n * 10) 
            fire(n); 
        else 
            (h = a[n - 1]).className = a[n - 1].className.replace("hinting_mode_hint", "hinting_mode_hint_focus"); 
    }; 
} 
function fire(n) 
{ 
    el = a[n - 1]; 
    tag = el.nodeName.toLowerCase(); 
    clear(); 
    if(tag == "iframe" || tag == "frame" || tag == "textarea" || (tag == "input" && el.type == "text")) 
        el.focus(); 
    else { 
        var evObj = document.createEvent('MouseEvents'); 
        evObj.initMouseEvent('click', true, true, window, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, null); 
        el.dispatchEvent(evObj); 
    } 
} 
function cleanup() 
{ 
    for(e in a) 
        a[e].className = a[e].className.replace(/hinting_mode_hint/,''); 
    div.parentNode.removeChild(div); 
    window.onkeyup = null; 
} 
function clear() 
{ 
    cleanup(); 
    console.log("hintmode_off") 
} 
