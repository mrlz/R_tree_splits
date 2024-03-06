#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define min(x, y) ({                \
  typeof(x) _min1 = (x);          \
  typeof(y) _min2 = (y);          \
  (void) (&_min1 == &_min2);      \
  _min1 < _min2 ? _min1 : _min2; })

  #define max(x, y) ({                \
    typeof(x) _max1 = (x);          \
    typeof(y) _max2 = (y);          \
    (void) (&_max1 == &_max2);      \
    _max1 > _max2 ? _max1 : _max2; })

    #define BILLION 1000000000L


    typedef struct rectangle{
      float minx;
      float miny;
      float maxx;
      float maxy;
    } Rectangle;

    float random_float(void){
      struct timespec start;
      clock_gettime(CLOCK_MONOTONIC, &start);
      srand(start.tv_nsec);
      return (float)rand()/(float)(RAND_MAX/500000.0);
    }


    int overlaps(Rectangle r1, Rectangle r2){
      if (r1.minx <= r2.maxx && r1.maxx >= r2.minx && r1.maxy >= r2.miny && r1.miny <= r2.maxy){
        return 1;
      }
      return 0;
    }

    int random_int(void){
      struct timespec start;
      clock_gettime(CLOCK_MONOTONIC, &start);
      srand(start.tv_nsec);
      int x = rand() / (RAND_MAX / 100 + 1);
      if (x == 0){
        return random_int();
      }
      return x;
    }

    typedef struct entries{
      Rectangle MBR;
      int child;
    } Entry;

    int m = 81;
    int M = 203;
    // int m = 2;
    // int M = 3;

    int read_from_disk = 0;
    int save_to_disk = 0;
    int stored = 0;
    int stored_leaves = 0;
    int number_of_leaves = 0;

    struct timespec startg, endg;
    struct timespec startr, endr;
    float dr, dg;

    int total_unlikely_reads = 0;
    int total_guaranteed_reads = 0;
    int total_random_reads = 0;

    uint64_t diffg,diffr;

    typedef struct node{
      Entry Information[203];
      int type;
      int currently_stored;
      int parent;
      int isRoot;
      int node_id;
    } Node;

    typedef struct tree{
      Node *Root;
      int minimum;
      int maximum;
      int created_nodes;
      int Root_id;
    } Tree;


    Node *Get_Node(int node_id){
      Node *N = malloc(sizeof(*N));
      char str[1000];
      sprintf(str, "%d", node_id);
      FILE *file = fopen(str, "rb");
      if(file != NULL){
        fread(N, sizeof(*N), 1, file);
        fclose(file);
        read_from_disk = read_from_disk + 1;
      }else{
        printf("trying to get node %d, non on disk\n",node_id);
      }
      return N;
    }

    void Save_Node(Node *N){
      char str[1000];
      sprintf(str, "%d", N->node_id);
      FILE *file = fopen(str, "wb");
      if (file != NULL){
        fwrite(N, sizeof(*N),1,file);
        fclose(file);
        save_to_disk = save_to_disk + 1;
      }else{
        printf("could not write to node %d\n", N->node_id);
      }
    }

    Node *new_node(int M, int t, int id){
      Node *new;
      new = (Node *)malloc(sizeof(*new));
      new->type = t; // t = 1 => leaf
      if (t == 1){
        number_of_leaves = number_of_leaves + 1;
      }
      new->currently_stored = 0;
      new->parent = -1;
      new->isRoot = 0;
      new->node_id = id;
      return new;
    }

    Tree *new_tree(int m, int M){
      Tree *new_tree;
      new_tree = (Tree *)malloc(sizeof(Tree));
      new_tree->Root = new_node(M, 1, 0);
      new_tree->Root->isRoot = 1;
      new_tree->minimum = m;
      new_tree->maximum = M;
      new_tree->created_nodes = 1;
      return new_tree;
    }

    void print_rectangle(Rectangle r){
      printf("(%.4f,",r.minx);
      printf("%.4f),",r.miny);
      printf("(%.4f,",r.maxx);
      printf("%.4f)\n",r.maxy);
    }

    void print_offset(int offset){
      int j = 0;
      while (j < offset){
        printf(" ");
        j = j + 1;
      }
    }


    Rectangle random_rectangle(void){
      int x = random_int();
      float yy = random_float();

      int y = random_int();
      float xx = random_float();

      Rectangle R1 = {xx,yy,xx + (float)x,yy + (float)y};
      return R1;
    }

    Rectangle compute_MBR_of_pair(Rectangle r1, Rectangle r2){
      float minx, miny, maxx, maxy;
      minx = min(r1.minx,r2.minx);
      miny = min(r1.miny,r2.miny);
      maxx = max(r1.maxx,r2.maxx);
      maxy = max(r1.maxy,r2.maxy);
      Rectangle R = {minx,miny,maxx,maxy};
      return R;
    }

    float New_Area(Rectangle MBR1, Rectangle MBR){
      Rectangle New = compute_MBR_of_pair(MBR1, MBR);
      float maxx = New.maxx-New.minx;
      float maxy = New.maxy-New.miny;
      return (maxx*maxy);
    }

    float Rectangle_Area(Rectangle MBR){
      return (MBR.maxx-MBR.minx)*(MBR.maxy-MBR.miny);
    }

    Node *ChooseLeaf(Node *N, Entry R){
      if (N->type == 1){ //isLeaf
        return N;
      }
      int number_of_leaves = N->currently_stored;
      float differences[number_of_leaves];
      float total_area[number_of_leaves];
      int i = 1;
      float new_area = New_Area(R.MBR,N->Information[0].MBR);
      float minimal_area = new_area;
      differences[0] = new_area - (N->Information[0].MBR.maxx-N->Information[0].MBR.minx)*(N->Information[0].MBR.maxy-N->Information[0].MBR.miny);
      total_area[0] = new_area;
      minimal_area = differences[0];
      int candidates[number_of_leaves];
      candidates[0] = 0;
      int selected_candidates = 1;
      while (i < number_of_leaves){
        Entry current = N->Information[i];
        new_area = New_Area(R.MBR, current.MBR);
        differences[i] = new_area - (current.MBR.maxx-current.MBR.minx)*(current.MBR.maxy-current.MBR.miny);
        total_area[i] = new_area;
        if (differences[i] < minimal_area){
          minimal_area = differences[i];
          selected_candidates = 1;
          candidates[0] = i;
        }else if(new_area == minimal_area){
          candidates[selected_candidates] = i;
          selected_candidates = selected_candidates + 1;
        }
        i = i + 1;
      }
      if (selected_candidates == 1){
        Node *next = Get_Node(N->Information[candidates[0]].child);
        if (N->isRoot == 0){
        free(N);
        }
        return ChooseLeaf(next,R);
      }
      int index = candidates[0];
      minimal_area = total_area[candidates[index]];
      i = 1;
      while (i < selected_candidates){
        if (total_area[candidates[i]] < minimal_area ){
          index = candidates[i];
        }
        i = i + 1;
      }
      //printf("leaf 10\n");
      Node *next = Get_Node(N->Information[index].child);
      if (N->isRoot == 0){
      free(N);
      }
      return ChooseLeaf(next,R);
    }

    Rectangle compute_MBR(Node *N){
      Entry *info = N->Information;
      int i = 1;
      float minx, miny, maxx, maxy;
      minx = info[0].MBR.minx;
      miny = info[0].MBR.miny;
      maxx = info[0].MBR.maxx;
      maxy = info[0].MBR.maxy;
      while (i < N->currently_stored){
        minx = min(minx,info[i].MBR.minx);
        miny = min(miny,info[i].MBR.miny);
        maxx = max(maxx,info[i].MBR.maxx);
        maxy = max(maxy,info[i].MBR.maxy);
        i = i + 1;
      }
      Rectangle R = {minx,miny,maxx,maxy};
      return R;
    }

    float area_difference(Rectangle MBR1, Rectangle MBR2){
      return New_Area(MBR1,MBR2) - Rectangle_Area(MBR1) - Rectangle_Area(MBR2);
    }

    int DecideGroup(float d1,float d2,float area1,float area2,int n_G1,int n_G2){
      if (d1 < d2){
        return 0;
      }else if(d1 > d2){
        return 1;
      }else{
        if (area1 < area2){
          return 0;
        }else if(area1 > area2){
          return 1;
        }else{
          if (n_G1 < n_G2){
            return 0;
          }else if (n_G1 > n_G2){
            return 1;
          }else{
            time_t t;
            srand((unsigned) time(&t));
            int r = rand() % 10;
            if (r < 5){
              return 0;
            }else{
              return 1;
            }
          }
        }
      }
    }

    void PickNext(Entry *E, int *index, Rectangle G1, Rectangle G2, int n_G1, int n_G2, int* s1, int* s2, int size){
      int i = 0;
      int candidate;
      float area1 = Rectangle_Area(G1);
      float area2 = Rectangle_Area(G2);
      float max = -1;
      while (i < size){
        if (s1[i] + s2[i] == 0){
        float d1 = New_Area(G1,E[i].MBR) - area1;
        float d2 = New_Area(G2,E[i].MBR) - area2;
        float diff = fabs(d1-d2);
        if (max == -1){
          max = diff;
          candidate = i;
          index[1] = DecideGroup(d1,d2,area1,area2,n_G1,n_G2);
        }

        if (diff > max){
          max = diff;
          candidate = i;
          index[1] = DecideGroup(d1,d2,area1,area2,n_G1,n_G2);
        }
      }
        i = i + 1;
    }
      index[0] = candidate;
    }


    void PickSeeds(Entry *E, int *indexes){
      //printf(" values = %d\n", indexes[0]);
      int i = 0;
      int j = 0;
      float max = area_difference(E[0].MBR,E[1].MBR);
      int first, second;
      while (i < indexes[0]){
        j = 0;
        while (j < indexes[0]){
          if (i != j){
          float dif = area_difference(E[i].MBR,E[j].MBR);
          if (dif >= max){ //area is probably never bigger than 0.0
            first = i;
            second = j;
            max = dif;
          }}
          j = j + 1;
        }
        i = i + 1;
      }
      indexes[0] = first;
      indexes[1] = second;
    }

    void Modify_Parent(int parent, int node_id){
      Node *N0 = Get_Node(node_id);
      N0->parent = parent;
      Save_Node(N0);
      free(N0);
    }

    void Quadratic_Split(Node *N, Entry E, Node *LL, Tree *T){
      int size = N->currently_stored+1;
      Entry collection[size];
      int sortedg1[size];
      int sortedg2[size];
      int i = 0;
      int g1 = 0;
      Rectangle MBR_g1;
      int g2 = 0;
      Rectangle MBR_g2;
      while (i < N->currently_stored){
        collection[i] = N->Information[i];
        sortedg1[i] = 0;
        sortedg2[i] = 0;
        i = i + 1;
      }
      sortedg1[N->currently_stored] = 0;
      sortedg2[N->currently_stored] = 0;
      collection[N->currently_stored] = E;
      int indexes[2] = {size, 0};
      //printf("quad index 0 %d index 1 %d",indexes[0], indexes[1]);
      PickSeeds(collection, indexes);
      sortedg1[indexes[0]] = 1;
      N->Information[g1] = collection[indexes[0]]; //instant addition
      if (N->type == 0){
      Modify_Parent(N->node_id, N->Information[g1].child);
      }
      g1 = 1;
      MBR_g1 = collection[indexes[0]].MBR;
      sortedg2[indexes[1]] = 1;
      LL->Information[g2] = collection[indexes[1]]; //instant addition
      if (N->type == 0){
      Modify_Parent(LL->node_id, LL->Information[g2].child);
      }
      g2 = 1;
      MBR_g2 = collection[indexes[1]].MBR;
      int c = 0;
      while(g1+g2 < size){
        if (size-g2 == T->minimum){
          while (c < size){
            if (sortedg1[c] + sortedg2[c] == 0){
              sortedg1[c] = 1;
              N->Information[g1] = collection[c];
              if (N->type == 0){
              Modify_Parent(N->node_id,N->Information[g1].child);
              }
              g1 = g1+1;
            }
            c = c + 1;
          }
        }else if(size-g1 == T->minimum){
          while (c < size){
            if (sortedg1[c]+sortedg2[c] == 0){
              sortedg2[c] = 1;
              LL->Information[g2] = collection[c];
              if (N->type == 0){
              Modify_Parent(LL->node_id, LL->Information[g2].child);
              }
              g2 = g2+1;
            }
            c = c + 1;
          }
        }else{
        PickNext(collection,indexes,MBR_g1,MBR_g2,g1,g2,sortedg1,sortedg2, size);
        if (indexes[1] == 0){ //goes into g1
          sortedg1[indexes[0]] = 1;
          N->Information[g1] = collection[indexes[0]]; //instant addition
          if (N->type == 0){
          Modify_Parent(N->node_id, N->Information[g1].child);
          }
          g1 = g1 + 1;
          MBR_g1 = compute_MBR_of_pair(MBR_g1,collection[indexes[0]].MBR);
        }else if (indexes[1] == 1){
          sortedg2[indexes[0]] = 1;
          LL->Information[g2] = collection[indexes[0]]; // instant addition
          if (N->type == 0){
          Modify_Parent(LL->node_id, LL->Information[g2].child);
          }
          g2 = g2 + 1;
          MBR_g2 = compute_MBR_of_pair(MBR_g2,collection[indexes[0]].MBR);
        }
      }
      }
      N->currently_stored = g1; //instant addition
      LL->currently_stored = g2; //instant addition
    }

    void print_node_info(int node_id){
      Node *L = Get_Node(node_id);
      print_rectangle(L->Information[0].MBR);
      print_rectangle(L->Information[1].MBR);
      printf("node id: %d current: %d parent: %d isroot: %d\n", L->node_id, L->currently_stored, L->parent, L->isRoot);

    }

    void AdjustTree(Tree *T, Node *L, Node *LL){
      if (L->isRoot == 1 && LL != NULL){
        Node *NR = new_node(T->maximum, 0, T->created_nodes);
        T->created_nodes = T->created_nodes + 1;
        L->isRoot = 0;
        L->parent = NR->node_id;
        LL->parent = NR->node_id;
        NR->isRoot = 1;
        Entry E1 = {compute_MBR(L),L->node_id};
        Entry E2 = {compute_MBR(LL),LL->node_id};
        NR->Information[0] = E1;
        NR->Information[1] = E2;
        NR->currently_stored = 2;
        T->Root = NR;
        Save_Node(L);
        Save_Node(LL);
        free(L);
        free(LL);
        return;
      }else if(L->isRoot == 1){
        return;
      }
      Node *P;
      if (L->parent != T->Root->node_id){
      P = Get_Node(L->parent);
    }else{
      P = T->Root;
    }
      int i = 0;
      while (i < P->currently_stored){
        if (P->Information[i].child == L->node_id){
          P->Information[i].MBR = compute_MBR(L);
          break;
        }
        i = i + 1;
      }

      if (LL != NULL){
        Save_Node(L);
        free(L);
        Entry E = {compute_MBR(LL),LL->node_id};
        if (P->currently_stored < T->maximum){
          P->Information[P->currently_stored] = E;
          P->currently_stored = P->currently_stored + 1;
          LL->parent = P->node_id;
          Save_Node(LL);
          free(LL);
          AdjustTree(T,P,NULL);
        }else{
        Node *LLL = new_node(T->maximum, 0, T->created_nodes); //if we're splitting a parent node the split produces 2 parents, I presume
        T->created_nodes = T->created_nodes + 1;
        Save_Node(LL);
        free(LL);
        Quadratic_Split(P, E, LLL, T);
        AdjustTree(T, P, LLL);
        }
      }else{
        Save_Node(L);
        free(L);
        AdjustTree(T,P,NULL);
      }
    }

    void Insert(Entry E, Tree* T){
      Node *candidate = ChooseLeaf(T->Root,E);
      if (candidate->currently_stored < T->maximum){
        candidate->Information[candidate->currently_stored] = E;
        candidate->currently_stored = candidate->currently_stored + 1;
        AdjustTree(T, candidate, NULL);
      }else{
        Node *LL = new_node(T->maximum, 1, T->created_nodes);
        T->created_nodes = T->created_nodes + 1; //node id before or after?
        Quadratic_Split(candidate, E, LL, T);
        AdjustTree(T, candidate, LL);
      }
    }

    void print_node(Node *N, int offset_size){
      int i = 0;
      while (i < N->currently_stored){
        if (N->isRoot == 1){
          printf("Root %d, ", N->node_id);
          //printf("ROOT %d, isRoot:%d currently_stored:%d node_type:%d ",N->node_id, N->isRoot,N->currently_stored, N->type); //additional info, uncomment
        }else{
          print_offset(offset_size);
          if(N->type == 0){
            printf("INNER %d, ", N->node_id);
          }else{
            printf("LEAF %d, Stored: %d ", N->node_id, N->Information[i].child);
          }
          //printf("parent:%d isRoot:%d currently_stored:%d node_type:%d ",N->parent, N->isRoot, N->currently_stored, N->type); //uncomment for additional info
        }
        print_rectangle(N->Information[i].MBR);
        if (N->type == 0){
        Node *c = Get_Node(N->Information[i].child);
        print_node(c, offset_size + 2);
        free(c);
        }
        i = i + 1;
      }
    }

    void currently_filled(Node *N){
      int i = 0;
      stored = stored + N->currently_stored;
      if (N->type == 1){
        stored_leaves = stored_leaves + N->currently_stored;
      }else{
      while (i < N->currently_stored){
        if (N->type == 0){
          Node *c = Get_Node(N->Information[i].child);
          currently_filled(c);
        }
        i = i + 1;
      }
    }
    }

    void print_tree(Tree* T){
      print_node(T->Root,0);
      printf("\n");
    }

    void Save_Tree(Tree *T){
      FILE *file = fopen("Tree", "wb");
      T->Root_id = T->Root->node_id;
      if (file != NULL){
        fwrite(T, sizeof(*T),1,file);
        fclose(file);
      }
    }

    Tree *Load_Tree(char *n){
      FILE *file = fopen(n,"rb");
      Tree *T = malloc(sizeof(*T));
      if (file !=NULL){
        fread(T, sizeof(*T),1,file);
        fclose(file);
      }
      T->Root = Get_Node(T->Root_id);
      return T;
    }

    void dump_tree(Tree* T){
      Save_Node(T->Root);
      Save_Tree(T);
      free(T);
    }

    // Entry *create_random_rectangles(int number_of_entries){ //NECESSARY TO CREATE TRULY RANDOM
    //   Entry *EE = malloc(number_of_entries*sizeof(Entry));
    //
    // }

    Entry *create_rectangles(int number_of_nodes){
    //Tree *tree = new_tree(m,M);
    Entry *EE = malloc(number_of_nodes*sizeof(Entry));
    float z = 499999.0;
    float max_height = 99.0;
    float max_width = 99.0;

    //este numero puede variar entre 512 y 33.554.432
    //int number_of_nodes = 512;
    int i = 0;
    for (i = 0; i < number_of_nodes; i++){
    float xmin = ((float)random()/(float)(RAND_MAX)) * z;
    float ymin = ((float)random()/(float)(RAND_MAX)) * z;

    if (z - xmin < 100.0){
    max_height = z - xmin;
    }

    if (z - ymin < 100.0){
    max_width = z - ymin;
    }
    float height = ((float)random()/(float)(RAND_MAX)) * max_height + 1.0;
    float width = ((float)random()/(float)(RAND_MAX)) * max_width + 1.0;

    float xmax = xmin + height;
    float ymax = ymin + height;

    Rectangle r = {xmin, ymin, xmax, ymax};
    Entry E;
    E.MBR = r;
    E.child = i;
    EE[i] = E;
    //printf("before insertion :");
    //Insert(E, tree);
    //printf("%d\n",i);
    }
      // printf("succesfully ended\n");
      //   dump_tree(tree);
    return EE;
    }

    void Search(Node *N, Rectangle S, int *entry_id, int slot){
      int i = 0;
      if (N->type != 1){
        while (i < N->currently_stored){
          if ( overlaps(S, N->Information[i].MBR) == 1){
            Node *next = Get_Node(N->Information[i].child);
            Search(next,S,entry_id,slot);
            free(next);
          }
          i = i + 1;
        }
      }else{
        while (i < N->currently_stored){
          if (overlaps(S, N->Information[i].MBR) == 1){
            int index = entry_id[slot];
            entry_id[index] = N->Information[i].child;
            entry_id[slot] = entry_id[slot] + 1;
          }
          i = i + 1;
        }
      }
    }

    void print_proposed_values(int *candidates, int num){
      int i = 0;
      while (i < candidates[num]){
        printf("%d ",candidates[i]);
        i = i + 1;
      }
      printf("\n");
    }


    int search_balanced_rectangles(Tree *tree, int search, Entry *EE, int num, int *ei,Rectangle *RR){
      int i = 0;
      printf("num = %d search = %d\n", num, search);
      // Rectangle R1 = {0.1,0.1,0.2,0.2};
      // Rectangle R2 = {499999.0,499999.0,500000.0,500000.0};
      int t = 0;
      // while (i < search){ //very unlikely to be on tree
      //   if (t == 0){
      //     Search(tree->Root,R1,ei,num);
      //     t = 1;
      //   }else{
      //     Search(tree->Root,R2,ei,num);
      //     t = 0;
      //   }
      //   ei[num] = 0;
      //   i = i + 1;
      // }
      // total_unlikely_reads = read_from_disk;

      clock_gettime(CLOCK_MONOTONIC, &startr);
      while (i < search){ //random
        Search(tree->Root,RR[i],ei,num);
        i = i + 1;
        ei[num] = 0;
      }
      clock_gettime(CLOCK_MONOTONIC, &endr);
      total_random_reads = read_from_disk;
      diffr = BILLION * (endr.tv_sec - startr.tv_sec) + endr.tv_nsec - startr.tv_nsec;
      dr = (float)diffr/1000000000.0;

      clock_gettime(CLOCK_MONOTONIC, &startg);
      while (i < 2*search){ //guaranteed to be on tree
        Search(tree->Root,EE[i].MBR,ei,num);
        i = i + 1;
        ei[num] = 0;
      }
      clock_gettime(CLOCK_MONOTONIC, &endg);
      total_guaranteed_reads = read_from_disk - total_random_reads;
      diffg = BILLION * (endg.tv_sec - startg.tv_sec) + endg.tv_nsec - startg.tv_nsec;
      dg = (float)diffg/1000000000.0;

      return i;
    }

    void insert_data_to_tree(Entry *EE, int num, int po){
      int i = 0;
      uint64_t diff;
      struct timespec start, end;
      total_unlikely_reads = 0;
      total_guaranteed_reads = 0;
      total_random_reads = 0;

      read_from_disk = 0;
      save_to_disk = 0;
      stored = 0;
      stored_leaves = 0;
      number_of_leaves = 0;
      save_to_disk = 0;
      read_from_disk = 0;

      clock_gettime(CLOCK_MONOTONIC, &start);
      Tree *tree = new_tree(m,M);
      while (i < num){
        Insert(EE[i],tree);
        i = i + 1;
      }
      clock_gettime(CLOCK_MONOTONIC, &end);

      diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
      //print_tree(tree);
      printf("\n|||||Tree of 2 to the %d|||||\n",po);
      printf("Elements = %d elapsed time for %d power = %llu nanoseconds\n", num,po, (long long unsigned int) diff);
      float d = (float)diff/1000000000.0;
      printf("Elements = %d elapsed time for %d power= %.3f seconds\n",num,po,d);
      printf("Each node is 4080 bytes, Number of nodes = %d, Total size = %d\n",tree->created_nodes, 4080*tree->created_nodes);
      currently_filled(tree->Root);
      float max = (float)tree->created_nodes*(float)M;
      float max_leaves = (float)number_of_leaves*(float)M;
      printf("Total node entries = %d of total possible entries %.5f | percentage = %.5f\n",stored, max, (float)stored/max);
      printf("Total leaf node entries = %d of total possible entries %.5f | percentage = %.5f\n", stored_leaves, max_leaves, (float)stored_leaves/max_leaves);
      printf("Total saves to disk %d | Total reads from disk %d\n",save_to_disk, read_from_disk);

      printf("\nSearch:\n");
      int *ei = malloc((num+1)*sizeof(int));
      i = 0;
      while (i < num+1){
        ei[i] = 0;
        i = i + 1;
      }

      float search = (float)num/10.0;
      search = search/2.0;

      //save_to_disk = 0;
      Rectangle *RandomRects = malloc((int)search*sizeof(Rectangle));
      int j = 0;
      while (j < (int)search){
        RandomRects[j] = random_rectangle();
        j = j + 1;
      }

      read_from_disk = 0;

      clock_gettime(CLOCK_MONOTONIC, &start);
      int s = search_balanced_rectangles(tree, (int)search, EE, num, ei,RandomRects);
      clock_gettime(CLOCK_MONOTONIC, &end);
      diff = BILLION * (end.tv_sec - start.tv_sec) + end.tv_nsec - start.tv_nsec;
      d = (float)diff/1000000000.0;
      printf("total reads %d || total searches %d || average reads %.4f || st %.3f s || st %llu nanos\n", read_from_disk, s, (float)read_from_disk/(float)s, d, (long long unsigned int)diff);
      printf("total guaranteed reads %d || total random reads %d\n",total_guaranteed_reads,total_random_reads);
      printf("guaranteed time %llu nanos|| random time %llu nanos|| avg guaranteed time %llu nanos|| avg random time %llu nanos\n", (long long unsigned int)diffg, (long long unsigned int)diffr ,(long long unsigned int)diffg/(long long unsigned int)search, (long long unsigned int)diffr/(long long unsigned int)search);
      printf("guaranteed time %.4f s || random time %.4f s || avg guaranteed time %.10f s || avg random time %.10f s\n", dg, dr, dg/search, dr/search);
      //print_proposed_values(ei,num);
      printf("|||||||||||||||||\n");

      dump_tree(tree);
      free(ei);
      free(EE);
      free(RandomRects);
    }

    void loop_same_insertion(int loops){
      Tree *tree = new_tree(m,M);
      Entry E1;
      Rectangle r1 = {212164.0, 292034.0, 212166.0, 292036.0};
      E1.MBR = r1;
      E1.child = 1;
      int i = 0;
      while (i < loops){
        Insert(E1, tree);
        i = i + 1;
      }
      dump_tree(tree);
    }

    void print_force_candidates(Rectangle r1, Entry *E, int num){
      int i = 0;
      printf("rectangle overalps with the following:\n");
      while (i < num){
        if (overlaps(r1,E[i].MBR) == 1){
          printf("%d for ",E[i].child);
          print_rectangle(E[i].MBR);
        }
        i = i + 1;
      }
    }

    void print_basic_example(){
      int print = 1;
      //int m = 2;
      Tree *tree = new_tree(m,M);
            Rectangle r1 = {2.0, 2.0, 4.0, 4.0};
            Rectangle r2 = {3.0, 5.0, 5.0, 6.0};
            Rectangle r3 = {5.0, 2.0, 6.0, 3.0};
            Rectangle r4 = {5.0, 4.0, 7.0, 5.0};
            Rectangle r5 = {0.0, 0.0, 1.0, 1.0};
            Rectangle r6 = {1.0, 0.0, 2.0, 4.0};
            Rectangle r7 = {2.0, 0.0, 3.0, 5.0};
            Rectangle r8 = {1.0, 6.0, 2.0, 7.0};
            Rectangle r9 = {5.0, 0.0, 7.0, 1.0};
            Rectangle r10 = {6.0, 0.0, 7.0, 4.0};
            Rectangle r11 = {3.0, 0.0, 4.0, 1.0};
            Rectangle r12 = {6.0, 6.0, 7.0, 7.0};

            Entry E1,E2,E3,E4,E5,E6,E7,E8,E9,E10,E11,E12;
            Entry EA[15];
            int a = 10;
            E1.child = 1;
            E2.child = 2;
            E3.child = 3;
            E4.child = 4;
            E5.child = 5;
            E6.child = 6;
            E7.child = 7;
            E8.child = 8;
            E9.child = 9;
            E10.child = 10;
            E11.child = 11;
            E12.child = 12;
            E1.MBR = r1; //creamos una entry
            E2.MBR = r2;
            E3.MBR = r3;
            E4.MBR = r4;
            E5.MBR = r5; //creamos una entry
            E6.MBR = r6;
            E7.MBR = r7;
            E8.MBR = r8;
            E9.MBR = r9; //creamos una entry
            E10.MBR = r10;
            E11.MBR = r11;
            E12.MBR = r12;
            Entry E13, E14, E15;
            Rectangle RR1={1.0,1.0,3.0,3.0};
            Rectangle RR2={2.0,2.0,3.0,3.0};
            Rectangle RR3={2.3,3.3,2.6,3.7};
            E13.MBR = RR1;
            E14.MBR = RR2;
            E15.MBR = RR3;
            E13.child = 13;
            E14.child = 14;
            E15.child = 15;

            EA[0] = E1;
            EA[1] = E2;
            EA[2]  = E3;
            EA[3] = E4;
            EA[4] = E5;
            EA[5] = E6;
            EA[6] = E7;
            EA[7] = E8;
            EA[8] = E9;
            EA[9] = E10;
            EA[10] = E11;
            EA[11] = E12;
            EA[12] = E13;
            EA[13] = E14;
            EA[14] = E15;

            Insert(E1,tree);
            if (print == 1){
            print_tree(tree);
            }
            Insert(E2, tree);
            if (print == 1){
            print_tree(tree);
            }

            Insert(E3, tree);
            if (print == 1){
            print_tree(tree);
            }

            Insert(E4, tree);
            if (print == 1){
            print_tree(tree);
            }

            Insert(E5, tree);
            if (print == 1){
            print_tree(tree);
            }

             Insert(E6, tree);
             if (print == 1){
             print_tree(tree);
             }

            Insert(E7, tree);
            if (print == 1){
            print_tree(tree);
            }

            Insert(E8, tree);
            if (print == 1){
            print_tree(tree);
            }


            Insert(E9, tree);
            if (print == 1){
            print_tree(tree);
            }

            Insert(E10, tree);
             if (print == 1){
             print_tree(tree);
             }

            Insert(E11, tree);
            if (print == 1){
            print_tree(tree);
             }

            //printf("12\n");
            Insert(E12, tree);
             if (print == 1){
            print_tree(tree);
             }
            //printf("13\n");

            //testing Search, Search is working properly
            Insert(E13,tree);
            if (print == 1){
           print_tree(tree);
            }
            Insert(E14,tree);
            if (print == 1){
           print_tree(tree);
            }
            Insert(E15,tree);
            if (print == 1){
           print_tree(tree);
            }
            //testing search
            int num = 15;
            int *ei = malloc((num+1)*sizeof(int));
            int i = 0;
            while (i < num+1){
              ei[i] = 0;
              i = i + 1;
            }

            read_from_disk = 0;

            printf("\n\nsearching ");
            print_rectangle(RR1);
            print_force_candidates(RR1,EA,num);
            Search(tree->Root,RR1,ei,num);
            printf("total reads: %d\nValues found ", read_from_disk);
            print_proposed_values(ei,num);
            printf("\n\n");

           i = 0;
            while (i < num+1){
              ei[i] = 0;
              i = i + 1;
            }

            read_from_disk = 0;

            printf("\n\nsearching ");
            print_rectangle(RR2);
            print_force_candidates(RR2,EA,num);
            Search(tree->Root,RR2,ei,num);
            printf("total reads: %d\nValues found ", read_from_disk);
            print_proposed_values(ei,num);
            printf("\n\n");

           i = 0;
            while (i < num+1){
              ei[i] = 0;
              i = i + 1;
            }

            read_from_disk = 0;

            printf("\n\nsearching ");
            print_rectangle(RR3);
            print_force_candidates(RR3,EA,num);
            Search(tree->Root,RR3,ei,num);
            printf("total reads: %d\nValues found ", read_from_disk);
            print_proposed_values(ei,num);
            printf("\n\n");



            dump_tree(tree);
            free(ei);
            print_tree(Load_Tree("Tree"));

    }



    int power(int base, int exponent){
      if (exponent == 0){
        return 1;
      }
      int i = 1;
      int acc = base;
      while (i < exponent){
        acc = acc*base;
        i = i + 1;
      }
      return acc;
    }

    int main(){

      int po = 9;

      while (po < 25){
        int p = power(2,po);
        Entry *E = create_rectangles(p);
        insert_data_to_tree(E, p, po);
        po = po + 1;
       }
      //print_basic_example();

        //print_tree(Load_Tree("Tree"));



    }
