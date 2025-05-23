#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "editgraph.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      animationTimer(nullptr)
{
    ui->setupUi(this);

    // Set focus border style for all widgets inside this form
    this->setStyleSheet(R"(
        QWidget:focus {
            border: 2px solid #377DFF;
            border-radius: 4px;
        }
    )");


    for (const auto& graph : program.graphs) {
        ui->MapSelectionCmb->addItem(QString::fromStdString(graph.name));
    }
    ui->MapSelectionCmb->setCurrentIndex(-1);

    connect(ui->MapSelectionCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onMapSelectionChanged);

    // updateCityComboBoxes();
    // ShowMap(0);
}

MainWindow::~MainWindow()
{
    if (animationTimer) {
        animationTimer->stop();
        delete animationTimer;
    }
    delete ui;
}

void MainWindow::on_exploreButton_clicked()
{
    QString selectedMap = ui->MapSelectionCmb->currentText().trimmed();
    if (selectedMap.isEmpty()) {
        QMessageBox::warning(this, "No Map Selected", "Please select a map before exploring the shortest path.");
        return;
    }

    ExploreMap* exploreMap = new ExploreMap(&program, this);
    exploreMap->setAttribute(Qt::WA_DeleteOnClose);
    exploreMap->show();
}

float max(float a,double b)
{
    if(a>b)
        return a;
    else
        return b;
}

void MainWindow::ShowMap(int index)
{


    if (index < 0 || index >= program.graphs.size() || !program.currentGraph) {
        return;
    }
    cityNodes.clear();
    edgeLines.clear();
    program.currentGraph = &program.graphs[index];

    QGraphicsScene* scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::black);
    ui->graphicsView->setScene(scene);

    // Force-directed layout setup
    map<string, QPointF> positions;
    map<string, QPointF> velocities;

    // Initialize cities with spread-out positions
    int cityCount = program.currentGraph->getAllCities().size();
    if (cityCount > 0) {
        // Create initial positions in a circle for better starting distribution
        double radius = min(ui->graphicsView->width(), ui->graphicsView->height()) * 0.35;
        double centerX = ui->graphicsView->width() / 2.0;
        double centerY = ui->graphicsView->height() / 2.0;

        int i = 0;
        for (const auto& city : program.currentGraph->getAllCities()) {
            // Position cities in a circle
            double angle = 2.0 * M_PI * i / cityCount;
            double x = centerX + radius * cos(angle);
            double y = centerY + radius * sin(angle);
            positions[city] = QPointF(x, y);
            velocities[city] = QPointF(0, 0);
            i++;
        }
    }

    // Calculate the average edge distance to use for scaling
    double totalDistance = 0.0;
    int edgeCount = 0;
    for (const auto& city : program.currentGraph->getAllCities()) {
        for (const auto& [neighbor, edgeData] : program.currentGraph->adj[city]) {
            totalDistance += edgeData.first;
            edgeCount++;
        }
    }
    double avgDistance = (edgeCount > 0) ? totalDistance / edgeCount : 100.0;
    double distanceScale = 200.0 / avgDistance; // Scale factor to make average edge ~200px

    // Run force-directed algorithm iterations
    const int ITERATIONS = 150;
    const double REPULSION = 10000.0;
    const double SPRING_K = 0.002;
    const double DAMPING = 0.85;

    for (int iter = 0; iter < ITERATIONS; iter++) {
        // Calculate forces for each node
        map<string, QPointF> forces;

        // Initialize forces to zero
        for (const auto& city : program.currentGraph->getAllCities()) {
            forces[city] = QPointF(0, 0);
        }

        // Apply repulsive forces between all nodes
        for (const auto& city1 : program.currentGraph->getAllCities()) {
            for (const auto& city2 : program.currentGraph->getAllCities()) {
                if (city1 != city2) {
                    QPointF delta = positions[city1] - positions[city2];
                    double distance = sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                    if (distance < 1.0) distance = 1.0; // Avoid division by very small numbers

                    // Repulsive force is inversely proportional to distance
                    double repulsionStrength = REPULSION / (distance * distance);
                    // Limit maximum repulsion
                    repulsionStrength = min(repulsionStrength, 200.0);

                    if (distance > 0) {
                        QPointF repulsion = delta * (repulsionStrength / distance);
                        forces[city1] += repulsion;
                    }
                }
            }
        }

        // Apply spring forces for edges
        for (const auto& city : program.currentGraph->getAllCities()) {
            for (const auto& [neighbor, edgeData] : program.currentGraph->adj[city]) {
                QPointF delta = positions[city] - positions[neighbor];
                double distance = sqrt(delta.x() * delta.x() + delta.y() * delta.y());
                if (distance < 1.0) distance = 1.0;

                // Spring force proportional to difference between actual and desired distance
                double edgeLength = edgeData.first;  // Distance in km
                double idealDist = edgeLength * distanceScale;

                // Apply nonlinear spring force
                double springFactor = -SPRING_K * (distance - idealDist);
                if (distance > 0) {
                    QPointF spring = delta * (springFactor / distance);
                    forces[city] += spring;
                }
            }
        }

        // Add a small force toward the center to prevent flying away
        double centerX = ui->graphicsView->width() / 2.0;
        double centerY = ui->graphicsView->height() / 2.0;
        for (const auto& city : program.currentGraph->getAllCities()) {
            QPointF toCenter = QPointF(centerX, centerY) - positions[city];
            double dist = sqrt(toCenter.x() * toCenter.x() + toCenter.y() * toCenter.y());
            if (dist > 300) { // Only apply if far from center
                forces[city] += toCenter * (0.01 * (dist - 300) / dist);
            }
        }

        // Update positions and velocities
        double maxSpeed = (iter < ITERATIONS / 2) ? 10.0 : 5.0; // Lower max speed later
        for (const auto& city : program.currentGraph->getAllCities()) {
            velocities[city] = (velocities[city] + forces[city]) * DAMPING;

            // Limit maximum velocity
            double speed = sqrt(velocities[city].x() * velocities[city].x() +
                                     velocities[city].y() * velocities[city].y());
            if (speed > maxSpeed) {
                velocities[city] *= (maxSpeed / speed);
            }

            positions[city] += velocities[city];

            // Ensure nodes stay within bounds with some padding
            double viewWidth = ui->graphicsView->width();
            double viewHeight = ui->graphicsView->height();
            double padding = 50.0;
            positions[city].setX(max(padding, min(viewWidth - padding, positions[city].x())));
            positions[city].setY(max(padding, min(viewHeight - padding, positions[city].y())));
        }

        // Slow things down in later iterations
        if (iter > ITERATIONS * 0.7) {
            for (auto& [city, vel] : velocities) {
                vel *= 0.95;
            }
        }
    }

    // Scale the layout to fit better in the view
    double minX = numeric_limits<double>::max();
    double minY = numeric_limits<double>::max();
    double maxX = numeric_limits<double>::lowest();
    double maxY = numeric_limits<double>::lowest();

    for (const auto& [city, pos] : positions) {
        minX = min(minX, pos.x());
        minY = min(minY, pos.y());
        maxX = max(maxX, pos.x());
        maxY = max(maxY, pos.y());
    }

    double scaleX = (maxX > minX) ? (ui->graphicsView->width() - 100) / (maxX - minX) : 1.0;
    double scaleY = (maxY > minY) ? (ui->graphicsView->height() - 100) / (maxY - minY) : 1.0;
    double scale = min(scaleX, scaleY);

    // Apply scaling and centering if needed
    if (scale < 0.9 || scale > 1.1) {
        double centerX = (minX + maxX) / 2;
        double centerY = (minY + maxY) / 2;
        double viewCenterX = ui->graphicsView->width() / 2;
        double viewCenterY = ui->graphicsView->height() / 2;

        for (auto& [city, pos] : positions) {
            // Center and scale
            double newX = (pos.x() - centerX) * scale + viewCenterX;
            double newY = (pos.y() - centerY) * scale + viewCenterY;
            positions[city] = QPointF(newX, newY);
        }
    }

    // Draw nodes (cities)
    for (const auto& city : program.currentGraph->getAllCities()) {
        QPointF pos = positions[city];
        CityNode* node = new CityNode(QRectF(pos.x()-15, pos.y()-15, 30, 30), QString::fromStdString(city));
        scene->addItem(node);
        // Store reference to the city node
        cityNodes[QString::fromStdString(city)] = node;
        QGraphicsTextItem* label = scene->addText(QString::fromStdString(city));
        label->setPos(pos.x() - label->boundingRect().width()/2, pos.y() + 20);
    }

    // Draw edges
    set<pair<string, string>> drawn;
    for (const auto& city : program.currentGraph->getAllCities()) {
        for (const auto& [neighbor, edgeData] : program.currentGraph->adj[city]) {
            if (drawn.count({neighbor, city}) == 0) {
                QPointF from = positions[city];
                QPointF to = positions[neighbor];
                EdgeLine* edge = new EdgeLine(QLineF(from.x(), from.y(), to.x(), to.y()),
                                              QString::fromStdString(city),
                                              QString::fromStdString(neighbor));
                scene->addItem(edge);
                QString fromCity = QString::fromStdString(city);
                QString toCity = QString::fromStdString(neighbor);
                edgeLines[{fromCity, toCity}] = edge;
                edgeLines[{toCity, fromCity}] = edge;
                // Add distance label with offset
                QPointF mid = (from + to) / 2;
                QLineF line(from, to);
                QLineF normal = line.normalVector();
                normal.setLength(15);
                QPointF labelPos = mid + QPointF(normal.dx(), normal.dy());

                QGraphicsTextItem* distLabel = scene->addText(QString::number(edgeData.first) + " km");
                distLabel->setPos(labelPos);
                distLabel->setDefaultTextColor(Qt::white);

                drawn.insert({city, neighbor});
                drawn.insert({neighbor, city});
            }
        }
    }
    // this is the timer for the animation
    if (!animationTimer) {
        animationTimer = new QTimer(this);
        connect(animationTimer, &QTimer::timeout, this, &MainWindow::animateTraversalStep);
    }
}


void MainWindow::resetGraphColors()
{
    for (auto node : cityNodes) {
        node->highlight(Qt::cyan);
    }
    for (auto edge : edgeLines) {
        edge->setPen(QPen(Qt::red, 3));
    }
}

void MainWindow::animateTraversal(const vector<string>& path)
{

    animationPath.clear();
    for (const auto& city : path) {
        animationPath.append(QString::fromStdString(city));
    }
    resetGraphColors();
    currentAnimationStep = 0;
    animationTimer->start(500);
}

void MainWindow::animateTraversalStep()
{
    if (currentAnimationStep >= animationPath.size()) {
        animationTimer->stop();
        QTimer::singleShot(1000, this, &MainWindow::resetGraphColors);
        return;
    }


    QString currentCity = animationPath[currentAnimationStep];


    if (cityNodes.contains(currentCity)) {
        cityNodes[currentCity]->highlight(Qt::green);
    }


    if (currentAnimationStep > 0) {
        QString prevCity = animationPath[currentAnimationStep - 1];
        QPair<QString, QString> edge = {prevCity, currentCity};

        if (edgeLines.contains(edge)) {
            edgeLines[edge]->setPen(QPen(Qt::red, 3));
        }
    }

    currentAnimationStep++;
}

void MainWindow::onMapSelectionChanged(int index)
{
    ui->start->clear();
    if (index < 0 || index >= program.graphs.size()) {
        program.currentGraph = nullptr;
        if (ui->graphicsView->scene()) {
            ui->graphicsView->scene()->clear();
        }
        return;
    }

    program.currentGraph = &program.graphs[index];
    ShowMap(index);

    ui->start->clear();
    ui->traversal->clear();
    for (const auto& city : program.currentGraph->getAllCities()) {
        ui->start->addItem(QString::fromStdString(city));
    }
    ui->start->setCurrentIndex(-1);
}

void MainWindow::on_addGraphButton_clicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add New Graph"),
                                         tr("Graph name:"), QLineEdit::Normal,
                                         "", &ok);

    if (ok) {
        if(name.isEmpty()) { QMessageBox::warning(this, "Error", "Graph name cannot be empty."); return; }
        int index1 = ui->MapSelectionCmb->findText(name);
        if (index1 >= 0) { QMessageBox::warning(this, "Error", "Graph name already exists. Use a unique name."); return; }

        program.addGraph(name.toStdString());
       updateGraphComboBox();

        // Ensure that it will show the newly created graph.
        int index = ui->MapSelectionCmb->findText(name);
        if (index >= 0) {
            ui->MapSelectionCmb->setCurrentIndex(index);
            program.currentGraph = &program.graphs[index];
            ShowMap(index);
        }

        QMessageBox::information(this, "Graph Added", "Graph added successfully.");
    }
}

void MainWindow::updateGraphComboBox() {
    ui->MapSelectionCmb->clear();

    for (const auto& g : program.graphs) {
        ui->MapSelectionCmb->addItem(QString::fromStdString(g.name));
    }

    ui->MapSelectionCmb->setCurrentIndex(-1);
    program.currentGraph = nullptr;

    if (ui->graphicsView->scene()) {
        ui->graphicsView->scene()->clear();
    }
}

void MainWindow::on_deleteGraphButton_clicked()
{
    QString name = ui->MapSelectionCmb->currentText();
    if (name.isEmpty()) return;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Delete Graph",
                                  "Are you sure you want to delete the graph '" + name + "'?",
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        bool deleted = program.deleteGraph(name.toStdString());
        if (deleted) {
            updateGraphComboBox();
            QMessageBox::information(this, "Deleted", "Graph deleted.");
        } else {
            QMessageBox::warning(this, "Error", "Graph not found.");
        }
    }
}

void MainWindow::on_BFS_clicked()
{
    if (!program.currentGraph) return;
    QString start = ui->start->currentText();
    if (start.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Start cannot be empty.");
        return;
    }
    auto graph = program.currentGraph->BFS(start.toStdString());
    if (graph.empty()) {
        ui->traversal->setText("No path found.");
        return;
    }
    QString result;
    for (size_t i = 0; i < graph.size(); ++i) {
        result += QString::fromStdString(graph[i]);
        if (i + 1 < graph.size())
            result += " --> ";
    }
    ui->traversal->setText(result);

    animateTraversal(graph);
}

void MainWindow::on_DFS_clicked()
{
    if (!program.currentGraph) return;
    QString start = ui->start->currentText();
    if (start.isEmpty()) {
        QMessageBox::warning(this, "Input Error", "Start cannot be empty.");
        return;
    }
    auto graph = program.currentGraph->DFS(start.toStdString());
    if (graph.empty()) {
        ui->traversal->setText("No path found.");
        return;
    }

    // Display the path in text format
    QString result;
    for (size_t i = 0; i < graph.size(); ++i) {
        result += QString::fromStdString(graph[i]);
        if (i + 1 < graph.size())
            result += " --> ";
    }
    ui->traversal->setText(result);
    animateTraversal(graph);
}

void MainWindow::on_editGraph_clicked(){
    QString selectedMap = ui->MapSelectionCmb->currentText().trimmed();
    if(selectedMap.isEmpty()){
        QMessageBox::warning(this, "No Map Selected", "Please select a map before editing.");
        return;
    }

    editGraph* edit = new editGraph(&program, this);
    edit->setAttribute(Qt::WA_DeleteOnClose);
    edit->show();

    connect(edit, &QObject::destroyed, this, [=]() {
        ui->editGraph->setEnabled(true);
    });
}

void MainWindow::on_saveBtn_clicked()
{
    program.saveGraphs();
    this->close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!program.isModified) {
        event->accept();
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "Save Changes",
        "Do you want to save the changes you made?",
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        program.saveGraphs();
        event->accept();
    } else {

        event->accept();
    }
}

