/*
  This file is part of MADNESS.

  Copyright (C) 2007,2010 Oak Ridge National Laboratory

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

  For more information please contact:

  Robert J. Harrison
  Oak Ridge National Laboratory
  One Bethel Valley Road
  P.O. Box 2008, MS-6367

  email: harrisonrj@ornl.gov
  tel:   865-241-3937
  fax:   865-572-0680


  $Id: worldstuff.cc 2825 2012-03-24 19:00:07Z jeff.science@gmail.com $
*/

#include <world/worldfwd.h>
#include <world/worldmem.h>
#include <world/worldtime.h>
#include <world/worldam.h>
#include <world/worldtask.h>
#include <world/worldgop.h>
#include <cstdlib>
#include <sstream>

/// \file worldstuff.cc
/// \brief Static variables/functions that must be linked in

#ifdef __CYGWIN__
#include <windows.h>
#endif

#define MPI_THREAD_STRING(level)  \
        ( level==MPI::THREAD_SERIALIZED ? "THREAD_SERIALIZED" : \
            ( level==MPI::THREAD_MULTIPLE ? "THREAD_MULTIPLE" : \
                ( level==MPI::THREAD_FUNNELED ? "THREAD_FUNNELED" : \
                    ( level==MPI::THREAD_SINGLE ? "THREAD_SINGLE" : "WTF" ) ) ) )

namespace madness {

#ifdef MADNESS_USE_BSEND_ACKS
    namespace {
        void * mpi_ack_buffer;
    }
#endif // MADNESS_USE_BSEND_ACKS

    static double start_cpu_time;
    static double start_wall_time;
    const int WorldAmInterface::NSEND;

    std::list<World*> World::worlds;
    unsigned long World::idbase = 0;

    static int finalizestate = 0;

    World::World(MPI::Intracomm& comm)
            : obj_id(1)          ///< start from 1 so that 0 is an invalid id
            , user_state(0)
            , mpi(*(new WorldMpiInterface(comm)))
            , am(* (new WorldAmInterface(*this)))
            , taskq(*(new WorldTaskQueue(*this)))
            , gop(* (new WorldGopInterface(*this)))
            , myrand_next(0)
    {
        worlds.push_back(this);
        srand();  // Initialize random number generator
        cpu_frequency();

        // Assign a globally (within COMM_WORLD) unique ID to this
        // world by assigning to each processor a unique range of indices
        // and broadcasting from node 0 of the current communicator.
        // Each process in COMM_WORLD is given unique ids for 10K new worlds
        if(idbase == 0 && rank()) {
            idbase = rank()*10000;
        }
        // The id of a new world is taken from the unique range of ids
        // assigned to the process with rank=0 in the sub-communicator
        if(rank() == 0) {
            _id = idbase++;
        }
        gop.broadcast(_id);
        gop.barrier();
        am.worldid = _id;

//        std::cout << "JUST MADE WORLD " << id() << std::endl
    }


    void World::args(int argc, char** argv) {
        for (int arg=1; arg<argc; ++arg) {
            if (strcmp(argv[arg],"-dx")==0) xterm_debug("objtest", 0);
//             if (strcmp(argv[arg],"-dam")==0) am.set_debug(true);
//            if (strcmp(argv[arg],"-dmpi")==0) mpi.set_debug(true);
//             if (strcmp(argv[arg],"-dref")==0) mpi.set_debug(true);
        }
    }

    World::~World() {
        if (finalizestate == 1) return;
        worlds.remove(this);
        delete &taskq;
        delete &gop;
        delete &am;
        delete &mpi;
    }

    void error(const char *msg) {
        std::cerr << "MADNESS: fatal error: " << msg << std::endl;
        MPI::COMM_WORLD.Abort(1);
    }

    void initialize(int argc, char** argv) {
        start_cpu_time = cpu_time();
        start_wall_time = wall_time();
#ifdef HAVE_PAPI
        initialize_papi();
#endif

        bool bind[3];
        int cpulo[3];

        const char* sbind = getenv("MAD_BIND");
        if (!sbind) sbind = MAD_BIND_DEFAULT;
        std::istringstream s(sbind);
        for (int i=0; i<3; ++i) {
            int t;
            s >> t;
            if (t < 0) {
                bind[i] = false;
                cpulo[i] = 0;
            }
            else {
                bind[i] = true;
                cpulo[i] = t;
            }
        }

        ThreadBase::set_affinity_pattern(bind, cpulo); // Decide how to locate threads before doing anything
        ThreadBase::set_affinity(0);         // The main thread is logical thread 0

#ifdef SERIALIZE_MPI
        int required = MPI::THREAD_SERIALIZED;
#else
        int required = MPI::THREAD_MULTIPLE;
#endif
        int provided = MPI::Init_thread(argc, argv, required);
        int me = MPI::COMM_WORLD.Get_rank();
        if (provided < required && me == 0 ) {
            std::cout << "!! Warning: MPI::Init_thread did not provide requested functionality "
                      << MPI_THREAD_STRING(required)
                      << " (" << MPI_THREAD_STRING(provided) << ")" << std::endl;
        }

        ThreadPool::begin();        // Must have thread pool before any AM arrives
        RMI::begin();               // Must have RMI while still running single threaded
        if (me == 0) std::cout << "Runtime initialized with " << ThreadPool::size() << " threads in the pool and affinity " << sbind << "\n";

#ifdef HAVE_PAPI
        begin_papi_measurement();
#endif

#ifdef MADNESS_USE_BSEND_ACKS
        mpi_ack_buffer = malloc(1000);
        MADNESS_ASSERT(mpi_ack_buffer != NULL);
        MPI::Attach_buffer(mpi_ack_buffer, 1000);
#endif // MADNESS_USE_BSEND_ACKS
    }

    void finalize() {
        RMI::end();
        ThreadPool::end(); // 8/Dec/08 : II added this line as trial
#ifdef MADNESS_USE_BSEND_ACKS
        MPI::Detach_buffer(mpi_ack_buffer);
        free(mpi_ack_buffer);
#endif // MADNESS_USE_BSEND_ACKS
        MPI::Finalize();
        finalizestate = 1;
    }

    // Enables easy printing of MadnessExceptions
    std::ostream& operator<<(std::ostream& out, const MadnessException& e) {
        out << "MadnessException : ";
        if (e.msg) out << "msg=" << e.msg << " : ";
        if (e.assertion) out << "assertion=" << e.assertion << " : ";
        out << "value=" << e.value << " : ";
        if (e.line) out << "line=" << e.line << " : ";
        if (e.function) out << "function=" << e.function << " : ";
        if (e.filename) out << "filename='" << e.filename << "'";
        out << std::endl;
        return out;
    }

    void exception_break(bool message) {
        if(message)
            std::cerr << "A madness exception occurred. Place a break point at madness::exception_break to debug.\n";
    }

    double wall_time() {
#ifdef __CYGWIN__
        static bool initialized = false;
        static double rfreq;
        if (!initialized) {
            _LARGE_INTEGER freq;
            if (QueryPerformanceFrequency(&freq))
                rfreq = 1.0/double(freq.QuadPart);
            else
                rfreq = 0.0;
            initialized = true;
        }
        _LARGE_INTEGER ins;
        QueryPerformanceCounter(&ins);
        return double(ins.QuadPart)*rfreq;
#else
        static bool first_call = true;
        static double start_time;

        struct timeval tv;
        gettimeofday(&tv,0);
        double now = tv.tv_sec + 1e-6*tv.tv_usec;

        if (first_call) {
            first_call = false;
            start_time = now;
        }
        return now - start_time;
#endif
    }

    double cpu_frequency() {
        static double freq = -1.0;
        if (freq == -1.0) {
            double used = wall_time();
            uint64_t ins = cycle_count();
            if (ins == 0) return 0;
            while ((cycle_count()-ins) < 100000000);  // 100M cycles at 1GHz = 0.1s
            ins = cycle_count() - ins;
            used = wall_time() - used;
            freq = ins/used;
        }
        return freq;
    }

    template <>
    std::ostream& operator<<(std::ostream& out, const Future<void>& f) {
        out << "<void>";
        return out;
    }

    template <>
    std::ostream& operator<<(std::ostream& out, const Future<Void>& f) {
        out << "<Void>";
        return out;
    }

    void print_stats(World& world) {
        double total_wall_time = wall_time()-start_wall_time;
        double total_cpu_time = cpu_time()-start_cpu_time;
        RMIStats rmi = RMI::get_stats();
        DQStats q = ThreadPool::get_stats();
#ifdef HAVE_PAPI
        // For papi ... this only make sense if done once after all
        // other worker threads have exited
        end_papi_measurement();
        const long long* values = get_papi_measurement();
#endif

        double nmsg_sent = rmi.nmsg_sent;
        double nmsg_recv = rmi.nmsg_recv;
        double nbyte_sent = rmi.nbyte_sent;
        double nbyte_recv = rmi.nbyte_recv;
        world.gop.sum(nmsg_sent);
        world.gop.sum(nmsg_recv);
        world.gop.sum(nbyte_sent);
        world.gop.sum(nbyte_recv);

        double max_nmsg_sent = rmi.nmsg_sent;
        double max_nmsg_recv = rmi.nmsg_recv;
        double max_nbyte_sent = rmi.nbyte_sent;
        double max_nbyte_recv = rmi.nbyte_recv;
        world.gop.max(max_nmsg_sent);
        world.gop.max(max_nmsg_recv);
        world.gop.max(max_nbyte_sent);
        world.gop.max(max_nbyte_recv);

        double min_nmsg_sent = rmi.nmsg_sent;
        double min_nmsg_recv = rmi.nmsg_recv;
        double min_nbyte_sent = rmi.nbyte_sent;
        double min_nbyte_recv = rmi.nbyte_recv;
        world.gop.min(min_nmsg_sent);
        world.gop.min(min_nmsg_recv);
        world.gop.min(min_nbyte_sent);
        world.gop.min(min_nbyte_recv);

        double npush_back = q.npush_back;
        double npush_front = q.npush_front;
        double npop_front = q.npop_front;
        double ntask = q.npush_back + q.npush_front;
        double nmax = q.nmax;
        world.gop.sum(npush_back);
        world.gop.sum(npush_front);
        world.gop.sum(npop_front);
        world.gop.sum(ntask);
        world.gop.sum(nmax);

        double max_npush_back = q.npush_back;
        double max_npush_front = q.npush_front;
        double max_npop_front = q.npop_front;
        double max_ntask = q.npush_back + q.npush_front;
        double max_nmax = q.nmax;
        world.gop.max(max_npush_back);
        world.gop.max(max_npush_front);
        world.gop.max(max_npop_front);
        world.gop.max(max_ntask);
        world.gop.max(max_nmax);

        double min_npush_back = q.npush_back;
        double min_npush_front = q.npush_front;
        double min_npop_front = q.npop_front;
        double min_ntask = q.npush_back + q.npush_front;
        double min_nmax = q.nmax;
        world.gop.min(min_npush_back);
        world.gop.min(min_npush_front);
        world.gop.min(min_npop_front);
        world.gop.min(min_ntask);
        world.gop.min(min_nmax);

#ifdef HAVE_PAPI
        double val[NUMEVENTS], max_val[NUMEVENTS], min_val[NUMEVENTS];
        for (int i=0; i<NUMEVENTS; ++i) {
            val[i] = max_val[i] = min_val[i] = values[i];
        }
        world.gop.sum(val, NUMEVENTS);
        world.gop.max(max_val, NUMEVENTS);
        world.gop.min(min_val, NUMEVENTS);
#endif

        if (world.rank() == 0) {
            printf("\n");
            printf("    Parallel environment\n");
            printf("    --------------------\n");
            printf("                  #nodes    %d\n", world.size());
            if (world.size() == 1) {
                printf("       #threads per node    %d+main = %d\n", int(ThreadPool::size()), int(ThreadPool::size()+1));
                printf("          #total threads    %d\n", int(ThreadPool::size()+1));
            }
            else {
                printf("       #threads per node    %d+main+server = %d\n", int(ThreadPool::size()), int(ThreadPool::size()+2));
                printf("          #total threads    %d\n", int(ThreadPool::size()+2)*world.size());
            }
            printf("\n");

            printf("  RMI message statistics (min / avg / max)\n");
            printf("  ----------------------\n");
            printf(" #messages sent per node    %.2e / %.2e / %.2e\n",
                   min_nmsg_sent, nmsg_sent/world.size(), max_nmsg_sent);
            printf("    #bytes sent per node    %.2e / %.2e / %.2e\n",
                   min_nbyte_sent, nbyte_sent/world.size(), max_nbyte_sent);
            printf(" #messages recv per node    %.2e / %.2e / %.2e\n",
                   min_nmsg_recv, nmsg_recv/world.size(), max_nmsg_recv);
            printf("    #bytes recv per node    %.2e / %.2e / %.2e\n",
                   min_nbyte_recv, nbyte_recv/world.size(), max_nbyte_recv);
            printf("        #msgs systemwide    %.2e\n", nmsg_sent);
            printf("       #bytes systemwide    %.2e\n", nbyte_sent);
            printf("\n");
            printf("  Thread pool statistics (min / avg / max)\n");
            printf("  ----------------------\n");
            printf("         #tasks per node    %.2e / %.2e / %.2e\n",
                   min_ntask, ntask/world.size(), max_ntask);
            printf("     #max q len per node    %.2e / %.2e / %.2e\n",
                   min_nmax, nmax/world.size(), max_nmax);
            printf("  #hi-pri tasks per node    %.2e / %.2e / %.2e\n",
                   min_npush_front, npush_front/world.size(), max_npush_front);
            printf("\n");
#ifdef HAVE_PAPI
            printf("         PAPI statistics (min / avg / max)\n");
            printf("         ---------------\n");
            for (int i=0; i<NUMEVENTS; ++i) {
                printf("  %3d   #events per node    %.2e / %.2e / %.2e\n",
                       i, min_val[i], val[i]/world.size(), max_val[i]);
            }
            for (int i=0; i<NUMEVENTS; ++i) {
                printf("  %3d #events systemwide    %.2e\n", i, val[i]);
            }
            if (total_wall_time > 0) {
                for (int i=0; i<NUMEVENTS; ++i) {
                    printf("  %3d   #op/s systemwide    %.2e\n", i, val[i]/total_wall_time);
                }
            }
            printf("\n");
#endif
#ifdef WORLD_GATHER_MEM_STATS
            world_mem_info()->print();
#endif

            printf("         Total wall time    %.1fs\n", total_wall_time);
            printf("         Total  cpu time    %.1fs\n", total_cpu_time);
            printf("\n");
        }
        world.gop.fence();
#ifdef WORLD_PROFILE_ENABLE
        WorldProfile::print(world);
#endif
        world.gop.fence();
    }

} // namespace madness
