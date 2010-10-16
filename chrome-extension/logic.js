var className = ".preview_icon";
var previewIcons = $$(className);

previewIcons.each(function (item){
    var newElement = new Element('span')
    newElement.set('text', 'yo');
    item.getParent().getParent().grab(newElement);
    item.destroy(); 
    });
