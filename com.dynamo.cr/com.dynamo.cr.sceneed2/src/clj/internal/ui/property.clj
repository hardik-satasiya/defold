(ns internal.ui.property
  (:require [clojure.string :as str]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [service.log :as log]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [internal.node :as in]
            [internal.query :as iq]
            [camel-snake-kebab :refer :all])
  (:import [org.eclipse.core.runtime IStatus Status]
           [org.eclipse.swt.widgets Composite]
           [org.eclipse.ui ISelectionListener]
           [org.eclipse.ui.forms.widgets FormToolkit]
           [com.dynamo.cr.properties Messages]))

(set! *warn-on-reflection* true)

(defnk aggregate-properties
  [properties]
  (into {} (for [node-prop-map        properties
                 [prop-name prop]     node-prop-map
                 :when (some-> prop :type t/property-visible)]
             [prop-name prop])))

(defn- niceify-label
  [k]
  (-> k
    name
    ->Camel_Snake_Case_String
    (str/replace "_" " ")))

(defnk passthrough-presenter-registry
  [presenter-registry]
  presenter-registry)

(defn presenter-for-property [node property]
  (dp/lookup-presenter (n/get-node-value node :presenter-registry) (:type property)))

(defn- attach-user-data
  [spec prop-name presenter path]
  (assoc spec
    :user-data {:presenter presenter :prop-name prop-name :path path}
    :children  (mapv (fn [[child-name child-spec]] [child-name (attach-user-data child-spec prop-name presenter (conj path child-name))]) (:children spec))))

(defn- attach-listeners
  [spec ui-event-listener]
  (assoc spec
    :listen   (zipmap (:listen spec) (repeat ui-event-listener))
    :children (mapv (fn [[child-name child-spec]] [child-name (attach-listeners child-spec ui-event-listener)]) (:children spec))))

(defn- control-spec
  [ui-event-listener prop-name presenter]
  (-> (merge (dp/control-for-property presenter))
      (attach-user-data prop-name presenter [])
      (attach-listeners ui-event-listener)))

(defn- property-control-strip
  [ui-event-listener [prop-name {:keys [presenter]}]]
  (let [label-text (niceify-label prop-name)]
    [[:label-composite {:type :composite
                        :layout {:type :stack}
                        :children [[:label      {:type :label :text label-text}]
                                   [:label-link {:type :hyperlink :text label-text :underlined true :on-click (fn [_] (prn "RESET " prop-name)) :foreground [0 0 255] :tooltip-text Messages/FormPropertySheetViewer_RESET_VALUE}]]}]
     [prop-name (control-spec ui-event-listener prop-name presenter)]
     [:dummy           {:type :label :layout-data {:exclude true}}]
     [:status-label    {:type :status-label :style :border :status Status/OK_STATUS :layout-data {:min-width 50 :exclude true}}]]))

(defn- property-page
  [control-strips]
  [:page-content
   {:type :composite
    :layout {:type :grid :margin-width 0 :columns [{:horizontal-alignment :left} {:horizontal-alignment :fill}]}
    :children control-strips}])

(defn- make-property-page
  [toolkit ui-event-listener properties-form properties]
  (ui/make-control toolkit (ui/widget properties-form [:form :composite])
    (property-page (mapcat #(property-control-strip ui-event-listener %) properties))))

(def empty-property-page
  [:page-content
   {:type :composite
    :layout {:type :grid :columns [{:horizontal-alignment :left}]}
    :children [[:no-selection-label {:type :label :text Messages/FormPropertySheetViewer_NO_PROPERTIES}]]}])

(defn- make-empty-property-page
  [toolkit properties-form]
  (ui/make-control toolkit (ui/widget properties-form [:form :composite]) empty-property-page))

(defn- settings-for-page
  [properties]
  {:children
   (for [[prop-name {:keys [presenter value]}] properties]
     [prop-name (dp/settings-for-control presenter value)])})

(defn- cache-key
  [properties]
  (map-vals :type properties))

(defn- lookup-or-create [cache key f & args]
  (-> cache
      (swap! (fn [cache]
                (if (contains? cache key)
                  cache
                  (assoc cache key (apply f args)))))
      (get key)))

(defn- attach-presenters
  [node content]
  (map-vals #(assoc % :presenter (presenter-for-property node %)) content))

(defn- refresh-property-page
  [{:keys [sheet-cache toolkit properties-form ui-event-listener] :as node}]
  (let [content (attach-presenters node (in/get-node-value node :content))
        key     (cache-key content)
        page    (lookup-or-create sheet-cache key make-property-page toolkit ui-event-listener properties-form content)]
    (ui/update-ui!      (get-in page [:page-content]) (settings-for-page content))
    (ui/bring-to-front! (ui/widget page [:page-content]))
    (ui/scroll-to-top!  (ui/widget properties-form [:form]))))

(defn- refresh-after-a-while
  [graph this transaction]
  (when (and (ds/is-modified? transaction this :content)
             (not (ds/is-deleted? transaction this))
             (:debouncer this))
    (t/signal (:debouncer this))))

(def gui
  [:form {:type   :form
          :text   "Properties"
          :layout {:type :grid :columns [{:horizontal-alignment :fill :vertical-alignment :fill}]}
          :children [[:composite
                      {:type :composite
                       :layout {:type :stack}}]]}])

(n/defnode PropertyView
  (input  properties [t/Properties])
  (output content s/Any aggregate-properties)

  (input  presenter-registry t/Registry :inject)
  (output presenter-registry t/Registry passthrough-presenter-registry)

  (property triggers n/Triggers (default [#'refresh-after-a-while]))

  (on :create
    (let [toolkit           (FormToolkit. (.getDisplay ^Composite (:parent event)))
          properties-form   (ui/make-control toolkit (:parent event) gui)
          sheet-cache       (atom {})
          ui-event-listener (ui/make-listener #(n/dispatch-message self :ui-event :ui-event %) [])]
      (lookup-or-create sheet-cache (cache-key {}) make-empty-property-page toolkit properties-form)
      (ds/set-property self
        :sheet-cache       sheet-cache
        :toolkit           toolkit
        :properties-form   properties-form
        :ui-event-listener ui-event-listener
        :debouncer         (ui/display-debouncer 100 #(refresh-property-page (ds/refresh self))))))

  (on :ui-event
    (let [ui-event (:ui-event event)
          {:keys [presenter prop-name path]} (ui/get-user-data (:widget ui-event))
          content (in/get-node-value self :content)
          page (get @(:sheet-cache self) (cache-key content))
          widget-subtree (get-in page [:page-content prop-name])]
      (if (identical? (:widget ui-event) (ui/widget widget-subtree path))
        (let [prop (get content prop-name)
              presenter-event (dp/presenter-event-map ui-event)
              result (dp/on-event presenter widget-subtree path presenter-event (:value prop))]
          (when-let [new-value (:value result)]
            (ds/set-property {:_id (:node-id prop)} prop-name new-value)))
        (log/warn :message "Expected event from widget on active property page"))))

  ISelectionListener
  (selectionChanged [this part selection]
    (let [current-inputs (iq/sources-of this :properties)]
      (when (not= @selection (map (comp :_id first) current-inputs))
        (ds/transactional
          (doseq [[source-node source-label] current-inputs]
            (ds/disconnect source-node source-label this :properties))
          (doseq [n @selection]
            (ds/connect {:_id n} :properties this :properties)))))))

(defn implementation-for
  [scope]
  (ds/transactional
    (ds/in scope
      (ds/add (n/construct PropertyView)))))

(defn get-control
  "This is called by the Java shim GenericPropertySheetPage. Not for other use."
  [property-view-node]
  (-> property-view-node
      ds/refresh
      :properties-form
      (ui/widget [:form])))
