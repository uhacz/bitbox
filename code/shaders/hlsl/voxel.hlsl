passes:
{
    voxel1024 =
    {
        vertex = "vs_main";
        pixel = "ps_main";
    };
}; #~header


/*
przesylamy tutaj index w gridzie i kolor index: 10:10:10, color: rgba
mamy tutaj tez zbindowanego mesha (box) 

na podstawie index'u obliczamy pozycje worldSpace, transformujemy wierzcholek box'a i przepisujemy kolor :)
*/

shared cbuffer MaterialData: register(b3)
{
    
};
