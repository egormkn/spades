//***************************************************************************
//* Copyright (c) 2015 Saint Petersburg State University
//* Copyright (c) 2011-2014 Saint Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//***************************************************************************

/*
 * tip_clipper.hpp
 *
 *  Created on: Mar 25, 2011
 *      Author: sergey
 */

#pragma once

#include <set>

#include "xmath.h"
#include "func.hpp"
#include "graph_support/basic_edge_conditions.hpp"
#include "graph_support/graph_processing_algorithm.hpp"

namespace omnigraph {

template<class Graph>
class RelativeCoverageTipCondition: public EdgeCondition<Graph> {
	typedef typename Graph::EdgeId EdgeId;
	typedef typename Graph::VertexId VertexId;
	typedef EdgeCondition<Graph> base;

	const double max_relative_coverage_;

	template<class IteratorType>
	double MaxCompetitorCoverage(EdgeId tip, IteratorType begin, IteratorType end) const {
		double result = 0;
		for (auto it = begin; it != end; ++it) {
			if (*it != tip)
				result = std::max(result, this->g().coverage(*it));
		}
		return result;
	}

	double MaxCompetitorCoverage(EdgeId tip) const {
		const Graph &g = this->g();
		VertexId start = g.EdgeStart(tip), end = g.EdgeEnd(tip);
		auto out = g.OutgoingEdges(start);
		auto in = g.IncomingEdges(end);
		return std::max(
						MaxCompetitorCoverage(tip, out.begin(),	out.end()),
						MaxCompetitorCoverage(tip, in.begin(), in.end()));
//		return std::max(
//				MaxCompetitorCoverage(tip, g.out_begin(start),
//						g.out_end(start)),
//				MaxCompetitorCoverage(tip, g.in_begin(end), g.in_end(end)));
	}

public:

	RelativeCoverageTipCondition(const Graph& g, double max_relative_coverage) :
			base(g), max_relative_coverage_(max_relative_coverage) {
	}

	bool Check(EdgeId e) const override {
		//+1 is a trick to deal with edges of 0 coverage from iterative run
		double max_coverage = MaxCompetitorCoverage(e) + 1;
		return math::le(this->g().coverage(e),
				max_relative_coverage_ * max_coverage);
	}
};

template<class Graph>
class TipCondition : public EdgeCondition<Graph> {
    typedef EdgeCondition<Graph> base;

    typedef typename Graph::EdgeId EdgeId;
    typedef typename Graph::VertexId VertexId;

    /**
     * This method checks if given vertex topologically looks like end of tip
     * @param v vertex to be checked
     * @return true if vertex judged to be tip and false otherwise.
     */
    bool IsTip(VertexId v) const {
        return this->g().IncomingEdgeCount(v) + this->g().OutgoingEdgeCount(v) == 1;
    }

public:
    TipCondition(const Graph& g) : base(g) {
    }

    /**
     * This method checks if given edge topologically looks like a tip.
     * @param edge edge vertex to be checked
     * @return true if edge judged to be tip and false otherwise.
     */
    bool Check(EdgeId e) const override {
        return (IsTip(this->g().EdgeEnd(e)) || IsTip(this->g().EdgeStart(e)))
                && (this->g().OutgoingEdgeCount(this->g().EdgeStart(e))
                        + this->g().IncomingEdgeCount(this->g().EdgeEnd(e)) > 2);
    }

};


template<class Graph>
class MismatchTipCondition : public EdgeCondition<Graph> {
    typedef EdgeCondition<Graph> base;
    typedef typename Graph::EdgeId EdgeId;
    typedef typename Graph::VertexId VertexId;

    size_t max_diff_;

    size_t Hamming(EdgeId edge1, EdgeId edge2) const {
        size_t len = std::min(this->g().length(edge1), this->g().length(edge2));
        size_t cnt = 0;
        Sequence seq1 = this->g().EdgeNucls(edge1);
        Sequence seq2 = this->g().EdgeNucls(edge2);
        for(size_t i = 0; i < len; i++) {
            if(seq1[i] != seq2[i])
                cnt++;
        }
        return cnt;
    }

    bool InnerCheck(EdgeId e) const {
        size_t len = this->g().length(e);
        for (auto alt : this->g().OutgoingEdges(this->g().EdgeStart(e))) {
            if (e != alt && len < this->g().length(alt) && Hamming(e, alt) <= max_diff_) {
                return true;
            }
        }
        return false;
    }

public:
    MismatchTipCondition(const Graph& g, size_t max_diff) : 
        base(g), max_diff_(max_diff) {
    }

    bool Check(EdgeId e) const override {
        return InnerCheck(e) || InnerCheck(this->g().conjugate(e));
    }

};

template<class Graph>
class ATCondition: public EdgeCondition<Graph> {
    typedef typename Graph::EdgeId EdgeId;
    typedef typename Graph::VertexId VertexId;
    typedef EdgeCondition<Graph> base;
    const double max_AT_percentage_;
    const size_t max_tip_length_;
    const bool check_tip_ ;

public:

    ATCondition(const Graph& g, double max_AT_percentage, size_t max_tip_length, bool check_tip) :
            base(g), max_AT_percentage_(max_AT_percentage), max_tip_length_(max_tip_length), check_tip_(check_tip) {
    }

    bool Check(EdgeId e) const {
        //+1 is a trick to deal with edges of 0 coverage from iterative run
        size_t start = 0;
        //TODO: Do we need this check?
        if(this->g().length(e) > max_tip_length_)
            return false;
        size_t end = this->g().length(e) + this->g().k();
        if (check_tip_) {
            if (this->g().OutgoingEdgeCount(this->g().EdgeEnd(e)) == 0)
                start = this->g().k();
            else if (this->g().IncomingEdgeCount(this->g().EdgeStart(e)) == 0)
                end = this->g().length(e);
            else return false;
        }
        std::array<size_t, 4> counts;
        const Sequence &s_edge = this->g().EdgeNucls(e);

        for (size_t position = start; position < end; position ++) {
            counts[s_edge[position]] ++;
        }
        size_t curm = *std::max_element(counts.begin(), counts.end());
        if (curm > (end - start) * max_AT_percentage_) {
            DEBUG("deleting edge" << s_edge.str());;
            DEBUG("start end cutoff" << start << " " << end << " " << this->g().length(e) * max_AT_percentage_);

            return true;
        } else {
            return false;
        }
    }

private:
    DECL_LOGGER("ATCondition")
};

template<class Graph>
pred::TypedPredicate<typename Graph::EdgeId> AddTipCondition(const Graph& g,
                                                            pred::TypedPredicate<typename Graph::EdgeId> condition) {
    return pred::And(TipCondition<Graph>(g), condition);
}

template<class Graph>
pred::TypedPredicate<typename Graph::EdgeId>
NecessaryTipCondition(const Graph& g, size_t max_length, double max_coverage) {
    return AddTipCondition(g, pred::And(LengthUpperBound<Graph>(g, max_length),
                                       CoverageUpperBound<Graph>(g, max_coverage)));
}

template<class Graph>
class DeadEndCondition : public EdgeCondition<Graph> {
    typedef EdgeCondition<Graph> base;

    typedef typename Graph::EdgeId EdgeId;
    typedef typename Graph::VertexId VertexId;

    /**
     * This method checks if given vertex topologically looks like end of tip
     * @param v vertex to be checked
     * @return true if vertex judged to be tip and false otherwise.
     */
    bool IsDeadEnd(VertexId v) const {
        return this->g().IncomingEdgeCount(v) * this->g().OutgoingEdgeCount(v) == 0;
    }

public:
    DeadEndCondition(const Graph& g) : base(g) {
    }

    /**
     * This method checks if given edge topologically looks like a tip.
     * @param edge edge vertex to be checked
     * @return true if edge judged to be tip and false otherwise.
     */
    /*virtual*/

    //Careful - no alternative path check!
    bool Check(EdgeId e) const {
        return (IsDeadEnd(this->g().EdgeEnd(e)) || IsDeadEnd(this->g().EdgeStart(e)))
                && (this->g().OutgoingEdgeCount(this->g().EdgeEnd(e))
                        + this->g().IncomingEdgeCount(this->g().EdgeStart(e)) >= 1);
    }

};

template<class Graph>
pred::TypedPredicate<typename Graph::EdgeId>AddDeadEndCondition(const Graph& g,
                                                                  pred::TypedPredicate<typename Graph::EdgeId> condition) {
    return pred::And<typename Graph::EdgeId>(
            make_shared<DeadEndCondition<Graph>>(g),
            condition);
}


//template<class Graph>
//bool ClipTips(
//        Graph& g,
//        size_t max_length,
//        shared_ptr<Predicate<typename Graph::EdgeId>> condition
//            = make_shared<func::AlwaysTrue<typename Graph::EdgeId>>(),
//        std::function<void(typename Graph::EdgeId)> removal_handler = 0) {
//
//    omnigraph::EdgeRemovingAlgorithm<Graph> tc(g,
//                                               AddTipCondition(g, condition),
//                                               removal_handler);
//
//    return tc.Run(LengthComparator<Graph>(g),
//                      make_shared<LengthUpperBound<Graph>>(g, max_length));
//}

} // namespace omnigraph
