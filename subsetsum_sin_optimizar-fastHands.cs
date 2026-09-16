using System;
using System.Collections.Generic;
using System.Linq;

string val;
int totalMemo = int.Parse(Console.ReadLine());
int spaceRequired = int.Parse(Console.ReadLine());
int quanityApps = int.Parse(Console.ReadLine());
int daysUmbral = int.Parse(Console.ReadLine());
List<App> apps = [];
apps.Add(null);
for (int i = 0; i < quanityApps; i++)
{
    val = Console.ReadLine();
    string[] data = val.Split(';');
    if (int.Parse(data[2]) < daysUmbral)
        continue;
    apps.Add(new App(data[0], data[1], data[2]));
}

bool[,] subset = new bool[apps.Count, spaceRequired + 1];

for (int i = 0; i < apps.Count; i++)
{
    subset[i, 0] = true;
}

for (int app = 1; app < apps.Count; app++)
{
    for (int subTarget = 1; subTarget <= spaceRequired; subTarget++)
    {
        if (apps[app].space > subTarget)
        {
            subset[app, subTarget] = subset[app - 1, subTarget];
        }
        else
        {
            subset[app, subTarget] =
                subset[app - 1, subTarget] || subset[app - 1, subTarget - apps[app].space];
        }
    }
}

// INFO: Toca recoonstruir el path

if (!subset[apps.Count - 1, spaceRequired])
{
    Console.WriteLine(false);
    return;
}
List<App> appsForDel = [];
makeListApps(apps.Count - 1, spaceRequired, appsForDel, subset, apps);
Console.WriteLine(true);
appsForDel.Reverse();
Console.WriteLine(string.Join(", ", appsForDel));

/*for (int i = 0; i < appsForDel.Count; i++)
{
    Console.WriteLine(string.Join(",", appsForDel));
    if (i == appsForDel.Count - 1)
        Console.Write($"{appsForDel[i]}");
    else
        Console.Write($"{appsForDel[i]},");
}*/

static void makeListApps(int row, int col, List<App> appsForDel, bool[,] subset, List<App> apps)
{
    if (row <= 0 || col <= 0)
        return;

    if (subset[row - 1, col])
        makeListApps(row - 1, col, appsForDel, subset, apps);
    else
    {
        appsForDel.Add(apps[row]);
        makeListApps(row - 1, col - apps[row].space, appsForDel, subset, apps);
    }
}

class App
{
    public string name;
    public int space;
    public int daysUnUsed;

    public App(string name, string space, string daysUnUsed)
    {
        this.name = name;
        this.space = int.Parse(space);
        this.daysUnUsed = int.Parse(daysUnUsed);
    }

    public override string ToString()
    {
        return $"{name}";
    }
}
