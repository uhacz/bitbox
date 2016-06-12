public class Attributes
{
    public class LocatorNode
    {
        public float3 pos = new float3(0.0f);
        public float3 rot = new float3(0.0f);
        public float3 scale = new float3(1.0f);
    }

    public class MeshNode : LocatorNode
	{
        public string mesh = ":box";
        public string material = "red";
	}
}
