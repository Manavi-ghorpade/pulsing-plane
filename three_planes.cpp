#include<notcurses/notcurses.h>
#include<time.h>
#include<pthread.h>
#include <algorithm>
#include<iostream>
using namespace std;
 struct shared_resources 
{
  struct ncplane* floating_plane[3];
  struct notcurses* nc_environ;
  pthread_mutex_t lock;
  int current_planeID; //0, 1, or 2
};

void* shrinking_box(void* _sh_res)
{
  struct shared_resources* res_ptr=(struct shared_resources*) _sh_res;
  int shrink_box_by=0;
  while(true)
  {
    pthread_mutex_lock(&res_ptr->lock);
    ncplane_erase(res_ptr->floating_plane[res_ptr->current_planeID]); //erase current plane
    unsigned int y, x;
    ncplane_dim_yx(res_ptr->floating_plane[res_ptr->current_planeID], &y, &x);
    ncplane_cursor_move_yx(res_ptr->floating_plane[res_ptr->current_planeID],shrink_box_by,shrink_box_by);
    shrink_box_by=++shrink_box_by%(std::min(y,x)/2);
    uint64_t channel{0};
    ncchannels_set_fg_rgb8(&channel, 0xff,0xff,0xff); 
    ncplane_rounded_box(res_ptr->floating_plane[res_ptr->current_planeID], 0,channel,y-shrink_box_by,x-shrink_box_by, 0);
    notcurses_render(res_ptr->nc_environ);
    pthread_mutex_unlock(&res_ptr->lock);
    //delay for 1/6 second
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    const uint64_t ONE_SECOND = 1000000000ull;
    deadline.tv_nsec += ONE_SECOND/6;
    // Correct the time to account for the second boundary
    if(deadline.tv_nsec >= ONE_SECOND) {
      deadline.tv_nsec -= ONE_SECOND;
      ++deadline.tv_sec;
    }
    while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL)<0){
    if(errno != EINTR) return NULL;
    }
  }
  return nullptr;
}


int main()
{
  notcurses_options ncopts{0};
  struct notcurses* nc_environ;
  nc_environ=notcurses_core_init(&ncopts,nullptr);

  struct shared_resources sh_res = {
  .nc_environ = nc_environ,
  .lock = PTHREAD_MUTEX_INITIALIZER,
  .current_planeID = 0
  };
  sh_res.nc_environ=nc_environ;

  struct ncplane* std_plane=notcurses_stdplane(nc_environ);
  nccell my_underlying_cell=NCCELL_CHAR_INITIALIZER(' ');
  nccell_set_bg_rgb8(&my_underlying_cell,0x44,0x11,0x44);
  ncplane_set_base_cell(std_plane,&my_underlying_cell);
  
  //first plane
  //
  struct ncplane_options plane_opts{0};
  plane_opts.rows=5+rand()%10;
  plane_opts.cols=10+rand()%20;
  plane_opts.y=20;
  plane_opts.x=3;
  struct ncplane* smaller_plane=ncplane_create(std_plane,&plane_opts);
  unsigned int red,green,blue;
  nccell_set_bg_rgb8(&my_underlying_cell,0xff,0x00,0x00);
  ncplane_set_fg_rgb8(smaller_plane,0xff,0x00,0x00);
  ncplane_set_base_cell(smaller_plane,&my_underlying_cell);

  sh_res.floating_plane[0]=smaller_plane;

  //second plane
  struct ncplane_options plane_opts1{0};
  plane_opts1.rows=5+rand()%10;
  plane_opts1.cols=10+rand()%20;
  plane_opts1.y=30;
  plane_opts1.x=10;
  struct ncplane* smaller_plane1=ncplane_create(std_plane,&plane_opts1);
 
  nccell_set_bg_rgb8(&my_underlying_cell,0x00,0xff,0x00);
  ncplane_set_fg_rgb8(smaller_plane1,0x00,0xff,0x00);
  ncplane_set_base_cell(smaller_plane1,&my_underlying_cell);

  sh_res.floating_plane[1]=smaller_plane1;

 //third plane

  struct ncplane_options plane_opts2{0};
  plane_opts2.rows=5+rand()%10;
  plane_opts2.cols=10+rand()%20;
  plane_opts2.y=10;
  plane_opts2.x=50;
  struct ncplane* smaller_plane2=ncplane_create(std_plane,&plane_opts2);

  my_underlying_cell=NCCELL_CHAR_INITIALIZER(' ');
  nccell_set_bg_rgb8(&my_underlying_cell,0x00,0x00,0xff);
  ncplane_set_fg_rgb8(smaller_plane2,0x00,0x00,0xff);
  ncplane_set_base_cell(smaller_plane2,&my_underlying_cell);

  sh_res.floating_plane[2]=smaller_plane2;
  
  //end common
  struct ncinput input{0};
  struct ncplane* current_plane=smaller_plane;  //assign current plane

  sh_res.current_planeID=0;
  pthread_t tid;
  pthread_create(&tid, NULL, shrinking_box, &sh_res);

  do
  {
    notcurses_get_blocking(nc_environ, &input);
    pthread_mutex_lock(&sh_res.lock);
    switch(input.id)
    {
      case 'j':
        ncplane_move_rel(sh_res.floating_plane[sh_res.current_planeID],1,0); //down
        break;
      case 'l':
        ncplane_move_rel(sh_res.floating_plane[sh_res.current_planeID],0,1); //right
        break;
      case 'k':
        ncplane_move_rel(sh_res.floating_plane[sh_res.current_planeID],-1,0); //up
        break;
      case 'h':
        ncplane_move_rel(sh_res.floating_plane[sh_res.current_planeID],0,-1); //left
        break;
      case ' ':
        if(sh_res.floating_plane[sh_res.current_planeID]==sh_res.floating_plane[0])
        {
          ncplane_erase(sh_res.floating_plane[0]);
          sh_res.current_planeID=1;
        }
        else if(sh_res.floating_plane[sh_res.current_planeID]==sh_res.floating_plane[1])
        {
          ncplane_erase(sh_res.floating_plane[1]);
          sh_res.current_planeID=2;
        }
        else if(sh_res.floating_plane[sh_res.current_planeID]==sh_res.floating_plane[2])
        {
          ncplane_erase(sh_res.floating_plane[2]);
          sh_res.current_planeID=0;
        }
        break;
      case 'z':
        ncplane_move_top(sh_res.floating_plane[sh_res.current_planeID]);
        break;
      case 'c':
        unsigned int a,b,c;
        a=rand()%256;
        b=rand()%256;
        c=rand()%256;
        nccell_set_bg_rgb8(&my_underlying_cell,a,b,c);
        ncplane_set_fg_rgb8(sh_res.floating_plane[sh_res.current_planeID],a,b,c);
        ncplane_set_base_cell(sh_res.floating_plane[sh_res.current_planeID],&my_underlying_cell);
        break;    
    }
    notcurses_render(nc_environ);
    pthread_mutex_unlock(&sh_res.lock);
  }while(input.id!='q');
  notcurses_stop(nc_environ);
}
