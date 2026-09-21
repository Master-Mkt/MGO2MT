using System.ComponentModel;
using System.Text.Json.Nodes;
namespace Mgo2Mt.WpnEditor;
internal sealed class PropertyAdapter(Func<JsonObject> source,Field[] fields,Action<Field,object?> set):ICustomTypeDescriptor
{
    public AttributeCollection GetAttributes()=>AttributeCollection.Empty;
    public string GetClassName()=>"武器設定";
    public string? GetComponentName()=>null;
    public TypeConverter GetConverter()=>new();
    public EventDescriptor? GetDefaultEvent()=>null;
    public PropertyDescriptor? GetDefaultProperty()=>null;
    public object? GetEditor(Type editorBaseType)=>null;
    public EventDescriptorCollection GetEvents()=>EventDescriptorCollection.Empty;
    public EventDescriptorCollection GetEvents(Attribute[]? attributes)=>EventDescriptorCollection.Empty;
    public PropertyDescriptorCollection GetProperties()=>new(fields.Select(f=>new Prop(f,source,set)).ToArray());
    public PropertyDescriptorCollection GetProperties(Attribute[]? attributes)=>GetProperties();
    public object GetPropertyOwner(PropertyDescriptor? pd)=>this;
    private sealed class Prop(Field definition,Func<JsonObject> source,Action<Field,object?> set):PropertyDescriptor(definition.Key,[new DisplayNameAttribute(definition.Label),new DescriptionAttribute(definition.Help)])
    {
        public override Type ComponentType=>typeof(PropertyAdapter);
        public override bool IsReadOnly=>false;
        public override Type PropertyType=>definition.Type;
        public override bool CanResetValue(object component)=>false;
        public override object GetValue(object? component)=>Fields.Get(source(),definition);
        public override void SetValue(object? component,object? value){set(definition,value);OnValueChanged(component,EventArgs.Empty);}
        public override void ResetValue(object component){}
        public override bool ShouldSerializeValue(object component)=>false;
    }
}
