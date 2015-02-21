#include "core/Layers.h"
#include "core/Softmax.h"

// Test file for LSTM
int main () {
	typedef double REAL_t;
	typedef Mat<REAL_t> mat;
    typedef std::shared_ptr<mat> shared_mat;
	using std::make_shared;
    using std::vector;

	LSTM<REAL_t> lstm(30, 50);
	Graph<REAL_t> G;

	auto embedding = make_shared<mat>(1000, 30, 2.0);

	auto prev_cell = make_shared<mat>(50, 1);
	auto prev_hidden = make_shared<mat>(50, 1);

	index_std_vector indices = {0, 1, 10, 2, 1, 3};

	auto out = lstm.activate(G, G.rows_pluck(embedding, indices), prev_cell, prev_hidden);

	out.first->print();

	// load numpy matrix from file:
    shared_mat numpy_mat;
    if (utils::file_exists("numpy_test.npy")) {
        numpy_mat = make_shared<mat>("numpy_test.npy");
    } else {
        numpy_mat = make_shared<mat>(3, 3);
        for (int i = 0; i < 9; i++) numpy_mat->w(i) = i;
        numpy_mat->npy_save("numpy_test.npy");
    }

	// print it
	numpy_mat->print();
	// take softmax
	auto softmaxed = softmax(numpy_mat);
	// print softmax:
	softmaxed->print();

	auto A = std::make_shared<mat>(3, 5);
    A->w = (A->w.array() + 1.2).matrix();
    // build random matrix of double type with standard deviation 2:
    auto B = std::make_shared<mat>(A->n, A->d, 2.0);
    auto C = std::make_shared<mat>(A->d, 4,    2.0);

    A->print();
    B->print();

    auto A_times_B    = G.eltmul(A, B);
    auto A_plus_B_sig = G.sigmoid(G.add(A, B));
    auto A_dot_C_tanh = G.tanh( G.mul(A, C) );
    auto A_plucked    = G.row_pluck(A, 2);

    A_times_B   ->print();
    A_plus_B_sig->print();
    A_dot_C_tanh->print();
    A_plucked   ->print();

    // add some random singularity and use exponential
    // normalization:
    A_plucked->w(2,0) += 3.0;
    auto A_plucked_normed = softmax(A_plucked);
    auto A_plucked_normed_t = softmax(G.transpose(A_plucked));
    A_plucked_normed->print();
    A_plucked_normed_t->print();

    // backpropagate to A and B
    G.backward();
    auto params = lstm.parameters();
    utils::save_matrices(params, "lstm_params");



    StackedInputLayer<REAL_t> superclassifier({20, 20, 10, 2}, 5);


    vector<shared_mat> inputs;
    inputs.emplace_back(make_shared<mat>(20, 5, -2.0, 2.0));
    inputs.emplace_back(make_shared<mat>(20, 5, -2.0, 2.0));
    inputs.emplace_back(make_shared<mat>(10, 5, -2.0, 2.0));
    inputs.emplace_back(make_shared<mat>(2,  5, -2.0, 2.0));

    auto out2 = superclassifier.activate(G, inputs);

    out2->print();


	return 0;
}
