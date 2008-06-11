/*
 * structure for the priority queue.  Also contains information for
 * smooth calculation.
 */
#include <queue>
#include "Compute.h"

class pqSmoothNode
{
 public:
    double fKey;	// distance squared -> place in priority queue
    Vector3D<double> dx; // displacement of this particle
    // XXX to be replaced with "ExternalSmoothParticle"
    ExternalGravityParticle *p; // pointer to rest of particle data
    
    inline bool operator<(const pqSmoothNode& n) const {
	return fKey < n.fKey;
	}
    };

	
// Object to bookkeep a Bucket Smooth Walk.

class NearNeighborState: public State {
public:
    std::priority_queue <pqSmoothNode> *Qs;
    NearNeighborState(int nParts) {
	Qs = new (std::priority_queue<pqSmoothNode>[nParts+2]);
	}
    ~NearNeighborState() { delete Qs; }
};

class SmoothCompute : public Compute 
{
    int nSmooth;
    void (*fcnSmooth)(GravityParticle *p, int nSmooth, pqSmoothNode *nList);
    void bucketCompare(TreePiece *tp,
		       ExternalGravityParticle *p,  // Particle to test
		       GenericTreeNode *node, // bucket
		       GravityParticle *particles, // local particle data
		       Vector3D<double> offset
		       ) ;
    
public:
 SmoothCompute(TreePiece *tp, void (*fcn)(GravityParticle *p, int nSmooth,
			   pqSmoothNode *nList), int nSm)
     : Compute(Smooth){
	nSmooth = nSm;
	fcnSmooth = fcn;
	state = new NearNeighborState(tp->myNumParticles+2);
	for (int j=0; j<tp->numBuckets; ++j) {
	    initSmoothPrioQueue(tp, j);
	    }
	}
    ~SmoothCompute() { delete state;}
	    
    void initSmoothPrioQueue(TreePiece *tp, int iBucket) ;

    bool openCriterion(TreePiece *ownerTP, GenericTreeNode *node, int reqID);
    
    int doWork(GenericTreeNode *node,
	       TreeWalk *tw,
	       State *state,
	       int chunk,
	       int reqID,
	       bool isRoot, 
	       bool &didcomp, int awi);
    int startNodeProcessEvent(TreePiece *owner){}
    int finishNodeProcessEvent(TreePiece *owner){}
    int nodeMissedEvent(TreePiece *owner, int chunk) {}
    State *getResumeState(int bucketIdx);
    void walkDone() ;
    };

void Density(GravityParticle *p,int nSmooth,pqSmoothNode *nnList);

class SmoothOpt : public Opt{
  public:
  SmoothOpt() : Opt(Local){
    // don't need to open
    // these nodes are your concern
    action_array[false][Internal] = DUMP;
    action_array[false][Bucket] = DUMP;

    action_array[false][Boundary] = DUMP; 
    action_array[false][NonLocal] = DUMP; 
    action_array[false][NonLocalBucket] = DUMP;	
    action_array[false][Cached] = DUMP;	
    action_array[false][CachedBucket] = DUMP;

    action_array[false][Empty] = DUMP;
    action_array[false][CachedEmpty] = DUMP;
    action_array[false][Top] = ERROR;
    action_array[false][Invalid] = ERROR;
    //--------------
    // need to open node
    // local data
    action_array[true][Internal] = KEEP;
    action_array[true][Bucket] = KEEP_LOCAL_BUCKET;
    action_array[true][Boundary] = KEEP;

    // remote data
    action_array[true][NonLocal] = KEEP;
    action_array[true][NonLocalBucket] = KEEP_REMOTE_BUCKET;
    action_array[true][CachedBucket] = KEEP_REMOTE_BUCKET;
    action_array[true][Cached] = KEEP;

    // discard
    action_array[true][Empty] = DUMP;
    action_array[true][CachedEmpty] = DUMP;
    action_array[true][Top] = ERROR;
    action_array[true][Invalid] = ERROR;
  }

};
