#ifndef RECURRENT_EMBEDDING_MODEL_MAT_H
#define RECURRENT_EMBEDDING_MODEL_MAT_H

#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include <chrono>

#include "dali/utils.h"
#include "dali/core.h"

template<typename R>
class RecurrentEmbeddingModel {
    public:
        typedef Mat<R>                      mat;
        typedef std::map<std::string, std::vector<std::string>> config_t;

        int vocabulary_size;
        const int output_size;
        const int stack_size;
        const int input_size;
        std::vector<int> hidden_sizes;

        mat embedding;
        virtual std::vector<int> reconstruct(
            Indexing::Index,
            int,
            int symbol_offset = 0) const = 0;
        virtual std::vector<utils::OntologyBranch::shared_branch> reconstruct_lattice(
            Indexing::Index,
            utils::OntologyBranch::shared_branch,
            int) const = 0;

        /**
        Configuration
        -------------
        Return a map with keys corresponding to hyperparameters for
        the model and where values are vectors of strings containing
        the assignments to each hyperparameter for the loaded model.

        Useful for saving the model to file and reloading it later.

        Outputs
        -------

        std::map<std::string, std::vector< std::string >> config : configuration map

        **/
        virtual config_t configuration() const;

        /**
        Parameters
        ----------

        Create a vector of shared pointers to the underlying matrices
        of the model. Useful for saving, loading parameters all at once
        and for telling Solvers which parameters should be updated
        during each training loop.

        Outputs
        -------

        std::vector<std::shared_ptr<Mat<T>>> parameters : vector of model parameters

        **/
        virtual std::vector<mat> parameters() const;

        void save(std::string) const;

        typedef std::vector<typename LSTM<R>::State> state_type;
        virtual state_type initial_states() const;
        /**
        Save Configuration
        ------------------
        Save model configuration as a text file with key value pairs.
        Values are vectors of string, and keys are known by the model.

        Input
        -----

        std::string fname : where to save the configuration

        **/
        virtual void save_configuration(std::string fname) const;
        std::string reconstruct_string(
            Indexing::Index,
            const utils::Vocab&,
            int,
            int symbol_offset = 0) const;
        std::string reconstruct_lattice_string(
            Indexing::Index,
            utils::OntologyBranch::shared_branch,
            int) const;
        RecurrentEmbeddingModel(
            int _vocabulary_size,
            int _input_size,
            int _hidden_size,
            int _stack_size,
            int _output_size);
        RecurrentEmbeddingModel(
            int _vocabulary_size,
            int _input_size,
            const std::vector<int>& _hidden_sizes,
            int _output_size);
        RecurrentEmbeddingModel(const RecurrentEmbeddingModel& model,
            bool copy_w,
            bool copy_dw);
        RecurrentEmbeddingModel(const config_t&);
};

// TODO(szymon): This piece of art should die eventually...
static Throttled model_save_throttled;
static int model_snapshot_no;

template<typename R> void maybe_save_model(RecurrentEmbeddingModel<R>* model,
                                           const std::string& base_path="",
                                           const std::string& label="");

#endif