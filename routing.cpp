
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <list>


#define MAX_ROW 100
#define MAX_COL 100
#define CALL_START 1
#define CALL_END 2 
#define SHPF 1
#define SDPF 2
#define LLP 3
#define MFC 4




int propdelay[MAX_ROW][MAX_COL];
int capacity[MAX_ROW][MAX_COL];
int available[MAX_ROW][MAX_COL];
float utilization[MAX_ROW][MAX_COL];
int cost_SHPF[MAX_ROW][MAX_COL];
int cost_SDPF[MAX_ROW][MAX_COL];



int number_nodes;
int original_events;

int total_hops = 0;
float total_delay = 0;
int num_events, calls, success, blocked;


FILE *fp1, *fp2;

/* Event record */
struct Event
{
    float event_time;
    int event_type;
    int callid;
    std::vector<int> route;
    char source;
    char destination;
    float duration;
  }; 



std::list<Event> EventList;
std::list<Event> OriginalList;


// Gets the node with the smallest distance away from the source node
// that has not yet been visited
int minDistance(float dist[],  
                bool visited[]) 
{ 
    float min = 999999;
    int min_index; 
  
    for (int v = 0; v < number_nodes; v++) 
        if (visited[v] == false && dist[v] <= min){
            min = dist[v]; 
            min_index = v;
        } 
  
    return min_index; 
}


// Reference https://www.geeksforgeeks.org/dijkstras-shortest-path-algorithm-greedy-algo-7/
// Djikstras algorithm, gets the path from source to destination
int djikstra(int src,int dst, Event *event, int algorithm) 
{ 
    int hops; 
    float dist[number_nodes];   
    bool visited[number_nodes]; 
    int path[number_nodes]; 
    

    for (int i = 0; i < number_nodes; i++) 
    { 
        if(i == src){
            path[src] = -1; 
            dist[i] = 0;
        }
        else
        {
            path[i] = -9999;
            dist[i] = 999999; 
        }
        visited[i] = false; 
    } 
    

    for (int count = 0; count < number_nodes; count++) 
    { 

        int u = minDistance(dist, visited); 
        visited[u] = true; 
        for (int v = 0; v < number_nodes; v++)
        {
            if (algorithm == SHPF && !visited[v]  && dist[u]+ cost_SHPF[u][v] <= dist[v] && available[u][v] > 0) // Shortest hop path first
            { 
                path[v] = u; 
                dist[v] = dist[u] + cost_SHPF[u][v]; 
            }
            else if (algorithm == SDPF && !visited[v]  && dist[u]+ cost_SDPF[u][v] < dist[v] && available[u][v] > 0) // shortest distance path first
            {
                path[v] = u; 
                dist[v] = dist[u] + cost_SDPF[u][v]; 
            }
            else if (algorithm == LLP && !visited[v]   && std::max(dist[u],utilization[u][v]) < dist[v] && available[u][v] > 0) // least loaded path 
            {
                path[v] = u; 
                dist[v] = std::max(dist[u],utilization[u][v]);
            }
            else if (algorithm == MFC && !visited[v]   && std::max(dist[u],(float)(1000 - available[u][v])) < dist[v] && available[u][v] > 0) // most free circuits
            {
                path[v] = u; 
                dist[v] = std::max(dist[u],(float)(1000 - available[u][v]));
            }     
        }

    } 

    if(path[dst]  == -9999) // no path exists
    {
        return 0;
    }
    hops = 0;
    int j = dst;
    int prev =0;
    float delay = 0;

    while(path[j] != -1)
    {
        event->route.push_back(j);
        prev = j;
        j = path[j];
        delay += propdelay[prev][j];
        hops++;
    }

    event->route.push_back(src);
    total_hops += hops;
    total_delay+= delay;
  /*printf("CALL %d: %c to %c\n",event->callid,src +'A',dst +'A');

    for(int i=0; i < event->route.size(); i++)
    printf("%c",event->route.at(i) + 'A');
  
    printf("\n");
    */
    return 1;

} 

// returns 0 if no route found, 1 if route found
int getBestRoute(Event *event,int algorithm)
{
  return djikstra(event->source - 'A',event->destination - 'A',event,algorithm);
}

// increases the load on the route
void increaseLoadOnRoute(std::vector<int> path)
{
    for(int i = 0; i < path.size() - 1; i++)
    {
        available[path.at(i)][path.at(i+1)] -= 1;
        available[path.at(i+1)][path.at(i)] -= 1;
        utilization[path.at(i)][path.at(i+1)] = (float (capacity[path.at(i)][path.at(i+1)] -  available[path.at(i)][path.at(i+1)]) / capacity[path.at(i)][path.at(i+1)]);
        utilization[path.at(i+1)][path.at(i)] = (float (capacity[path.at(i+1)][path.at(i)] -  available[path.at(i+1)][path.at(i)]) / capacity[path.at(i+1)][path.at(i)]);
    }
}

// decreases the load on the route
void decreaseLoadOnRoute(std::vector<int> path)
{
    for(int i = 0; i < path.size() - 1; i++)
    {
        available[path.at(i)][path.at(i+1)] += 1;
        available[path.at(i+1)][path.at(i)] += 1;
        utilization[path.at(i)][path.at(i+1)] = (float (capacity[path.at(i)][path.at(i+1)] -  available[path.at(i)][path.at(i+1)]) / capacity[path.at(i)][path.at(i+1)]);
        utilization[path.at(i+1)][path.at(i)] = (float (capacity[path.at(i+1)][path.at(i)] -  available[path.at(i+1)][path.at(i)]) / capacity[path.at(i+1)][path.at(i)]);
    }
}

// simulates algorithm
void simulateNetwork(int algorithm)
{
    total_delay = 0;
    total_hops = 0;
    EventList = OriginalList;
    num_events = original_events;
    int current_event = 0;
    success = 0;
    blocked = 0;
    std::list<Event>:: iterator it;
    for(it = EventList.begin(); it != EventList.end(); it++) // iterate list of events
    {
        Event *event;
        event = &*it;

        if(event->event_type == CALL_START)
        {   
            if(getBestRoute(event,algorithm) > 0)
            {
                increaseLoadOnRoute(event->route);
                success++;
                float depart_time = event->event_time + event->duration;
          
                Event e;
                e.event_type = CALL_END;
                e.event_time = event->event_type;
                e.source = event->source;
                e.route = event->route;
                e.callid = event->callid;
                e.destination = event->destination;
                e.duration = 0;
                std::list<Event>::iterator i;
                for(std::list<Event>::iterator iter = EventList.begin() ; iter != EventList.end(); iter++)
                {
                    float t = iter->event_time;
                    if(depart_time < t)
                    {
                        EventList.insert(iter,e);
                        goto cont;
                    }
                }
                EventList.push_back(e);
                cont:
                current_event++;
            }
            else
            {
                blocked++;
                current_event++;
            }
        }
        else if(event->event_type == CALL_END)
        { 
            decreaseLoadOnRoute(event->route);
            current_event++;
        }
    }


    if(algorithm == SHPF)
    {
        printf(" SHPF    %d      %d(%.2f%%)     %3d(%.2f%%)    %.3f   %.3f\n",calls,success,100*(float)success/calls,blocked,
        100*(float)blocked/calls,(float)total_hops/success,(float)total_delay/success);
    }
    else if(algorithm == SDPF)
    {
        printf(" SDPF    %d      %d(%.2f%%)     %3d(%.2f%%)    %.3f   %.3f\n",calls,success,100*(float)success/calls,blocked,
        100*(float)blocked/calls,(float)total_hops/success,(float)total_delay/success);
    }
    else if(algorithm == LLP)
    {
        printf(" LLP     %d      %d(%.2f%%)     %3d(%.2f%%)    %.3f   %.3f\n",calls,success,100*(float)success/calls,blocked,
        100*(float)blocked/calls,(float)total_hops/success,(float)total_delay/success);
    }
    else if(algorithm == MFC)
    {
        printf(" MFC     %d      %d(%.2f%%)     %3d(%.2f%%)    %.3f   %.3f\n",calls,success,100*(float)success/calls,blocked,
        100*(float)blocked/calls,(float)total_hops/success,(float)total_delay/success);
    }
    
}


int main()
{ 
    int  row, col,i,j, type, callid;
    char src,dst;
    int delay,cap;
    float arrival, duration; 
 
    // read in network topology
    fp1 = fopen("topology.dat", "r");
    number_nodes = 0;
    while( (i = fscanf(fp1, "%c %c %d %d\n", &src, &dst, &delay, &cap)) == 4 )
    {
	      row = src - 'A'; col = dst - 'A';
        if(std::max(row,col) >= number_nodes)
        {
            number_nodes = std::max(row,col) + 1;
        } 
        // store information
        propdelay[row][col] = delay; propdelay[col][row] = delay;
        capacity[row][col] = cap; capacity[col][row] = cap;
        available[row][col] = cap; available[col][row] = cap;
        cost_SHPF[row][col] = 1; cost_SHPF[col][row]= 1;
        cost_SDPF[row][col] = delay; cost_SDPF[col][row]= delay;
        utilization[row][col] = 0; utilization[col][row]= 0;
      }
    fclose(fp1);
   
    // Read in events
    fp2 = fopen("callworkload.dat","r");
    if(fp2 == NULL)
    {
        fprintf(stderr, "Unable to open callworkload.dat\n");
    }

    num_events = 0; // number of events
    callid = 0; // callid

    // store events into a list
    while( (i = fscanf(fp2, "%f %c %c %f\n", &arrival, &src, &dst, &duration)) == 4)
    {
        EventList.push_back(Event());
        auto it = std::next(EventList.begin(), num_events);
        it->event_type = CALL_START;
        it->event_time = arrival;
        it->source = src;
        it->callid = callid;
        it->destination = dst;
        it->duration = duration;
        num_events++;
        calls++;
        callid++;    
    }
    fclose(fp2);

    // store original number of events and eventlist
    original_events = num_events;
    OriginalList = EventList;

    
    printf("Policy   Calls       Success (%%)     Blocked (%%)   Hops    Delay\n");
    printf("------------------------------------------------------------------\n");
    simulateNetwork(SHPF);
    simulateNetwork(SDPF);
    simulateNetwork(LLP);
    simulateNetwork(MFC);



  }
